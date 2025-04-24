//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
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

OptionMapEntry::OptionMapEntry(const std::string &option,
                               const std::string &field_label,
                               const std::string &description,
                               const OptionValueType type)
    : option(option), field_label(field_label), description(description),
      exclusive_group(""),
      type(type), present(false), present_value(false), value("")
{
}

OptionMapEntry::OptionMapEntry(const std::string &option,
                               const std::string &field_label,
                               const std::string &exclusive_group,
                               const std::string &description,
                               const OptionValueType type)
    : option(option), field_label(field_label), description(description),
      exclusive_group(exclusive_group),
      type(type), present(false), present_value(false), value("")
{
}



//
//   class Configuration::File
//

File::File(const fs::path &fname)
{
    config_filename = fname;
}


const std::string File::GetFilename() const
{
    return config_filename;
}


void File::Parse(Json::Value &config)
{
    configure_mapping();

    // Copy the values and set the present flag in the
    // configured OptionMap (map) for JSON fields being recognised
    for (const auto &elm : config.getMemberNames())
    {
        auto it = std::find_if(map.begin(),
                               map.end(),
                               [elm](OptionMapEntry &e)
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
            if (config[elm].isString())
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


void File::Load(const fs::path &cfgfile)
{
    std::string fname = cfgfile.empty() ? config_filename : cfgfile;
    if (fname.empty())
    {
        throw ConfigFileException("No configuration filename provided");
    }

    std::ifstream cfgs(fname);
    if (cfgs.eof() || cfgs.fail())
    {
        throw ConfigFileException(fname, "Could not open file");
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
    catch (const Json::Exception &excp)
    {
        throw ConfigFileException(fname, "Error parsing file:" + std::string(excp.what()));
    }
}


std::vector<std::string> File::GetOptions(bool all_configured)
{
    configure_mapping();

    std::vector<std::string> opts;
    for (const auto &e : map)
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


bool File::IsPresent(const std::string &key)
{
    configure_mapping();

    auto it = std::find_if(map.begin(),
                           map.end(),
                           [key](OptionMapEntry &e)
                           {
                               return key == e.option;
                           });
    if (it == map.end())
    {
        throw OptionNotFound(key);
    }

    return it->present;
}


const std::string File::GetValue(const std::string &key)
{
    configure_mapping();

    auto it = std::find_if(map.begin(),
                           map.end(),
                           [key](OptionMapEntry &e)
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


const int File::GetIntValue(const std::string &key)
{
    return std::stoi(File::GetValue(key));
}


const bool File::GetBoolValue(const std::string &key)
{
    return (File::GetValue(key) == "true");
}


void File::SetValue(const std::string &key, const std::string &value)
{
    configure_mapping();

    auto it = std::find_if(map.begin(),
                           map.end(),
                           [key](OptionMapEntry &e)
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


void File::SetValue(const std::string &key, const int value)
{
    File::SetValue(key, std::to_string(value));
}


void File::SetValue(const std::string &key, const bool value)
{
    File::SetValue(key, std::string(value ? "true" : "false"));
}


void File::UnsetOption(const std::string &key)
{
    auto it = std::find_if(map.begin(),
                           map.end(),
                           [key](OptionMapEntry &e)
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
    for (const auto &e : map)
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
    for (const auto &grp : groups)
    {
        std::vector<std::string> used;
        for (const auto &opt : grp.second)
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
    auto s = std::find_if(map.begin(),
                          map.end(),
                          [option](OptionMapEntry &e)
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
    for (const auto &m : map)
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
    for (const auto &e : map)
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
            std::string comment = std::string("//  Option --")
                                  + e.option + " :: " + e.description;
            ret[e.field_label].setComment(comment,
                                          Json::CommentPlacement::commentBefore);
        }
    }
    return ret;
}


void File::Save(const fs::path &cfgfname)
{
    std::string fname = cfgfname.empty() ? config_filename : cfgfname;
    if (fname.empty())
    {
        throw ConfigFileException("No configuration filename provided");
    }

    std::ofstream cfgfile(fname);
    if (!empty())
    {
        cfgfile << Generate() << std::endl;
    }
    if (cfgfile.fail())
    {
        throw ConfigFileException(fname, "Error saving the configuration file");
    }
    cfgfile.close();
}


bool File::empty() const
{
    for (const auto &e : map)
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
    for (const auto &e : map)
    {
        std::cout << e;
    }
    std::cout << "---- DUMP -- EOF ----" << std::endl;
}
#endif
