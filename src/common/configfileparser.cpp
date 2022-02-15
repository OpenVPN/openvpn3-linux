//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   configfileparser.cpp
 *
 * @brief  Simple JSON based configuration file parser, targeting to
 *         integrate easily with the SingleCommand command line parser
 */

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "common/configfileparser.hpp"

#ifdef OPENVPN_DEBUG
#include <iostream>
#endif

using namespace Configuration;

//
//   struct Configuration::OptionMapEntry
//

OptionMapEntry::OptionMapEntry(std::string option, std::string field_label,
                               std::string description, OptionValueType type)
    : option(option), field_label(field_label), description(description),
      exclusive_group(""),
      type(type), present(false), present_value(false), value("")
{
}


OptionMapEntry::OptionMapEntry(std::string option, std::string field_label,
                               std::string exclusive_group,
                               std::string description, OptionValueType type)
    : option(option), field_label(field_label), description(description),
      exclusive_group(exclusive_group),
      type(type), present(false), present_value(false), value("")
{
}


//
//   class Configuration::File
//

File::File()
{
}


void File::Parse(Json::Value& config)
{
    configure_mapping();

    // Copy the values and set the present flag in the
    // configured OptionMap (map) for JSON fields being recognised
    for (const auto& elm : config.getMemberNames())
    {
        auto it = std::find_if(map.begin(), map.end(),
                               [elm](OptionMapEntry e)
                               {
                                    return elm == e.field_label;
                               });
        if (map.end() == it)
        {
            // Field not found, skip it
            continue;
        }
        it->present = true;
        if (OptionValueType::Present == it->type)
        {
            if( config[elm].isString())
            {
                // backwards compat parsing.
                // May be removed some time after the v25 release
                it->present_value = true;
            }
            else
            {
                it->present_value = config[elm].asBool();
            }
        }
        else
        {
            it->value = config[elm].asString();
        }
    }
}


void File::Load(const std::string& cfgfile)
{
    std::ifstream cfgs(cfgfile);
    if (cfgs.eof() || cfgs.fail())
    {
        throw ConfigFileException(cfgfile, "Could not open file");
    }

    // Read the configuration file
    std::string line;
    std::stringstream buf;
    while (std::getline(cfgs, line))
    {
        buf << line << std::endl;
    }

    // Don't try to parse an empty file
    if (0 == buf.tellp())
    {
        return;
    }

    // Parse the configuration file
    Json::Value jcfg;
    try
    {
        buf >> jcfg;
        Parse(jcfg);
    }
    catch (const Json::Exception& excp)
    {
        throw ConfigFileException(cfgfile, "Error parsing file:"
                                  + std::string(excp.what()));
    }
}


std::vector<std::string> File::GetOptions(bool all_configured)
{
    configure_mapping();

    std::vector<std::string> opts;
    for (const auto& e : map)
    {
        if (e.present || all_configured)
        {
            if ((OptionValueType::Present == e.type)
                || (!e.value.empty()) || all_configured)
            {
                opts.push_back(e.option);
            }
        }
    }
    return opts;
}


bool File::IsPresent(const std::string& key)
{
    configure_mapping();

    auto it = std::find_if(map.begin(), map.end(),
                           [key](OptionMapEntry e)
                           {
                                return key == e.option;
                           });
    if (it == map.end())
    {
        throw OptionNotFound(key);
    }

    return it->present;
}


std::string File::GetValue(const std::string& key)
{
    configure_mapping();

    auto it = std::find_if(map.begin(), map.end(),
                           [key](OptionMapEntry e)
                           {
                                return key == e.option;
                           });
    if (it == map.end())
    {
        throw OptionNotFound(key);
    }
    if (!it->present)
    {
        throw OptionNotPresent(key);
    }

    if (OptionValueType::Present == it->type)
    {
        return (it->present_value ? "true" : "false");
    }
    return it->value;
}


void File::SetValue(const std::string& key, const std::string& value)
{
    configure_mapping();

    auto it = std::find_if(map.begin(), map.end(),
                           [key](OptionMapEntry e)
                           {
                                return key == e.option;
                           });
    if (it == map.end())
    {
        throw OptionNotFound(key);
    }

    if (OptionValueType::Present == it->type)
    {
        it->present = true;
        it->present_value = ("1" == value || "yes" == value || "true" == value);
        it->value = "";
    }
    else
    {
        it->value = value;
        it->present = !value.empty();
    }
}


void File::UnsetOption(const std::string& key)
{
    auto it = std::find_if(map.begin(), map.end(),
                           [key](OptionMapEntry e)
                           {
                                return key == e.option;
                           });
    if (it == map.end())
    {
        throw OptionNotFound(key);
    }

    it->present = false;
    it->present_value = false;
    it->value = "";
}


void File::CheckExclusiveOptions()
{

    //  Create a map of all exclusive groups and
    //  which options each belongs to
    std::map<std::string, std::vector<std::string>> groups;
    for (const auto& e : map)
    {
        if (e.exclusive_group.empty())
        {
            // Ignore empty group values
            continue;
        }
        groups[e.exclusive_group].push_back(e.option);
    }

    //  Go through each option in each group and
    //  check if a value is present for that option
    for (const auto& grp : groups)
    {
        std::vector<std::string> used;
        for (const auto& opt : grp.second)
        {
            if (IsPresent(opt))
            {
                // track each option which is
                // used in this exclusive group
                used.push_back(opt);
            }
        }

        // If more than one option has been used
        // in this exclusive group, complain
        if (used.size() > 1)
        {
            throw ExclusiveOptionError(used);
        }
    }
}


std::vector<std::string> File::GetRelatedExclusiveOptions(const std::string option)
{
    configure_mapping();

    std::vector<std::string> ret{};

    // Find the option in the mapping
    auto s = std::find_if(map.begin(), map.end(),
                          [option](OptionMapEntry e)
                          {
                            return option == e.option;
                          });

    if (map.end() == s || s->exclusive_group.empty())
    {
        // Option not found => no related options
        return ret;
    }

    // Find all other options in the same option group as
    // the option just looked up
    for (const auto& m : map)
    {
        // Ignore empty exclusive_groups or itself
        if (m.exclusive_group.empty() || m.option == option)
        {
            continue;
        }

        // If it is the same exclusive option group ...
        if (s->exclusive_group == m.exclusive_group)
        {
            ret.push_back(m.option);
        }
    }
    return ret;
}


Json::Value File::Generate()
{
    configure_mapping();

    Json::Value ret;
    for (const auto& e : map)
    {
        if (e.present)
        {
            if (OptionValueType::Present == e.type)
            {
                ret[e.field_label] = e.present_value;
            }
            else
            {
                ret[e.field_label] = e.value;
            }
            std::string comment = std::string("//  Option --") + e.option + " :: " + e.description;
            ret[e.field_label].setComment(comment, Json::CommentPlacement::commentBefore);
        }
    }
    return ret;
}


void File::Save(const std::string cfgfname)
{
    std::ofstream cfgfile(cfgfname);
    if (!empty())
    {
        cfgfile << Generate() << std::endl;
    }
    if (cfgfile.fail())
    {
        throw ConfigFileException(cfgfname, "Error saving the configuration file");
    }
    cfgfile.close();
}


bool File::empty() const
{
    for (const auto& e : map)
    {
        if (e.present)
        {
            return false;
        }
    }
    return true;
}


void File::configure_mapping()
{
    if (!map_configured)
    {
        map = ConfigureMapping();
        map_configured = true;
    }
}

#ifdef OPENVPN_DEBUG
void File::Dump() const
{
    std::cout << "-------- DUMP -------" << std::endl;
    for (const auto& e : map)
    {
        std::cout << e;
    }
    std::cout << "---- DUMP -- EOF ----" << std::endl;
}
#endif
