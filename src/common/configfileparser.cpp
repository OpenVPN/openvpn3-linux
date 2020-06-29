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
      type(type), present(false), value("")
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
        it->value = config[elm].asString();
    }
}


void File::Load(const std::string& cfgfile)
{
    std::ifstream cfgs(cfgfile);
    if (cfgs.eof() || cfgs.fail())
    {
        throw ConfigFileException(cfgfile, "Could not open file");
    }

    // Parse the configuration file
    std::string line;
    std::stringstream buf;
    while (std::getline(cfgs, line))
    {
        buf << line << std::endl;
    }

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
        it->present = ("1" == value || "yes" == value);
        it->value = "";
    }
    else
    {
        it->value = value;
        it->present = !value.empty();
    }
    return;
}


Json::Value File::Generate()
{
    configure_mapping();

    Json::Value ret;
    for (const auto& e : map)
    {
        if (e.present)
        {
            ret[e.field_label] = e.value;
            std::string comment = std::string("//  Option --") + e.option + " :: " + e.description;
            ret[e.field_label].setComment(comment, Json::CommentPlacement::commentBefore);
        }
    }
    return ret;
}


void File::Save(const std::string cfgfname)
{
    if (empty())
    {
        return;
    }
    std::ofstream cfgfile(cfgfname);
    cfgfile << Generate() << std::endl;
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
