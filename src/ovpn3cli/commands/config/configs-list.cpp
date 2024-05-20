//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020-  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file   config.hpp
 *
 * @brief  Implementation of all the various openvpn3 config-* commands
 */

#include "build-config.h"
#include <json/json.h>
#include <gdbuspp/connection.hpp>

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "../../arghelpers.hpp"


/**
 *  openvpn3 config-list command
 *
 *  Lists all available configuration profiles.  Only profiles where the
 *  calling user is the owner, have been added to the access control list
 *  or profiles tagged with public_access will be listed.  This restriction
 *  is handled by the Configuration Manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_configs_list(ParsedArgs::Ptr args)
{
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    OpenVPN3ConfigurationProxy confmgr(dbuscon,
                                       Constants::GenPath("configuration"));

    args->CheckExclusiveOptions({{"filter-tag", "filter-owner"},
                                 {"json", "verbose"},
                                 {"json", "count"}});

    DBus::Object::Path::List config_list = {};
    if (args->Present("filter-tag"))
    {
        try
        {
            config_list = confmgr.SearchByTag(args->GetValue("filter-tag", 0));
        }
        catch (const DBus::Exception &err)
        {
            throw CommandException("configs-list", err.GetRawError());
        }
    }
    else if (args->Present("filter-owner"))
    {
        config_list = confmgr.SearchByOwner(args->GetValue("filter-owner", 0));
    }
    else
    {
        config_list = confmgr.FetchAvailableConfigs();
    }


    std::string filter_cfgname = {};
    if (args->Present("filter-config"))
    {
        filter_cfgname = args->GetValue("filter-config", 0);
    }

    bool verbose = args->Present("verbose");
    bool only_count = args->Present("count");
    bool json = args->Present("json");
    if (!only_count && !json)
    {
        // Print list header to the terminal
        if (verbose)
        {
            std::cout << "Configuration path" << std::endl;
            std::cout << "Imported" << std::setw(32 - 8) << std::setfill(' ') << " "
                      << "Last used" << std::setw(26 - 9) << " "
                      << "Used"
                      << std::endl;
            std::cout << "Configuration Name" << std::setw(58 - 18) << " "
                      << "Owner"
                      << std::endl;
            if (confmgr.CheckFeatures(CfgMgrFeatures::TAGS))
            {
                std::cout << "Tags"
                          << std::endl;
            }
        }
        else
        {
            std::cout << "Configuration Name" << std::setw(58 - 18) << std::setfill(' ') << " "
                      << "Last used"
                      << std::endl;
        }
        std::cout << std::setw(32 + 26 + 18 + 2) << std::setfill('-') << "-" << std::endl;
    }

    // List configuration profiles to the terminal
    bool first = true;
    uint32_t cfgcount = 0;
    Json::Value jsoncfgs;
    for (auto &cfg : config_list)
    {
        if (cfg.empty())
        {
            continue;
        }
        OpenVPN3ConfigurationProxy cprx(dbuscon, cfg, true);

        std::string name = cprx.GetName();
        if (!filter_cfgname.empty()
            && name.substr(0, filter_cfgname.length()) != filter_cfgname)
        {
            continue;
        }
        ++cfgcount;

        if (only_count)
        {
            continue;
            // We don't want to print any config details in this mode
        }

        std::string last_used = cprx.GetLastUsed();
        std::string user = lookup_username(cprx.GetOwner());
        std::string imported = cprx.GetImportTime();
        uint32_t used_count = cprx.GetUsedCounter();

        if (!json)
        {
            if (!verbose)
            {
                std::cout << name << std::setw(58 - name.size()) << std::setfill(' ') << " "
                          << (last_used.length() > 0 ? last_used : "-")
                          << std::endl;
            }
            else
            {
                if (!first)
                {
                    std::cout << std::endl;
                }
                first = false;

                std::cout << cfg << std::endl;
                std::cout << imported << std::setw(32 - imported.size()) << std::setfill(' ') << " "
                          << last_used << std::setw(26 - last_used.size()) << " "
                          << std::to_string(used_count)
                          << std::endl;
                std::cout << name << std::setw(58 - name.size()) << " " << user
                          << std::endl;

                if (confmgr.CheckFeatures(CfgMgrFeatures::TAGS))
                {
                    std::cout << "Tags: ";
                    size_t l = 6;
                    size_t tc = 0;
                    for (const auto &t : cprx.GetTags())
                    {
                        if (l > 6)
                        {
                            std::cout << ", ";
                        }
                        l += t.length() + 2;
                        if (l > 78)
                        {
                            l = 6;
                            std::cout << std::endl
                                      << "      ";
                        }
                        std::cout << t;
                        ++tc;
                    }
                    std::cout << (0 == tc ? "(none)" : "") << std::endl;
                }
            }
        }
        else
        {
            std::time_t imp_tstamp = cprx.GetImportTimestamp();
            std::time_t last_u_tstamp = cprx.GetLastUsedTimestamp();
            Json::Value jcfg;
            jcfg["name"] = name;
            jcfg["imported_tstamp"] = (Json::Value::UInt64)imp_tstamp;
            jcfg["imported"] = imported;
            jcfg["lastused_tstamp"] = (Json::Value::UInt64)last_u_tstamp;
            jcfg["lastused"] = last_used;
            jcfg["use_count"] = used_count;

            if (confmgr.CheckFeatures(CfgMgrFeatures::TAGS))
            {
                for (const auto &t : cprx.GetTags())
                {
                    jcfg["tags"].append(t);
                }
            }
            jcfg["dco"] = cprx.GetDCO();
            jcfg["transfer_owner_session"] = cprx.GetTransferOwnerSession();

            Json::Value acl;
            acl["owner"] = user;
            acl["locked_down"] = cprx.GetLockedDown();
            acl["public_access"] = cprx.GetPublicAccess();
            for (const auto &a : cprx.GetAccessList())
            {
                Json::Value d;
                acl["granted_access"].append(lookup_username(a));
            }
            jcfg["acl"] = acl;

            jsoncfgs[cfg] = jcfg;
        }
    }
    if (!only_count && !json)
    {
        std::cout << std::setw(32 + 26 + 18 + 2) << std::setfill('-')
                  << "-" << std::endl;
    }
    else if (json)
    {
        if (jsoncfgs.empty())
        {
            std::cout << "{}" << std::endl;
        }
        else
        {
            std::cout << jsoncfgs << std::endl;
        }
    }
    else
    {
        std::cout << "Configurations found: " << std::to_string(cfgcount) << std::endl;
    }
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'config-list' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_configs_list()
{
    //
    //  config-list command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("configs-list",
                                "List all available configuration profiles",
                                cmd_configs_list));

    cmd->AddOption("count",
                   "Only report the number of configurations found");
    cmd->AddOption("json",
                   "Format the output as JSON");
    cmd->AddOption("verbose",
                   'v',
                   "Provide more information about each configuration profile");
    cmd->AddOption("filter-config",
                   "NAME-PREFIX",
                   true,
                   "Filter configurations starting with the given prefix");
    cmd->AddOption("filter-tag",
                   "TAG-VALUE",
                   true,
                   "Only list configurations with the given tag");
    cmd->AddOption("filter-owner",
                   "OWNER",
                   true,
                   "Only list configurations belonging to a user");

    return cmd;
}
