//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020 - 2023  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file   config.hpp
 *
 * @brief  Implementation of all the various openvpn3 config-* commands
 */

#include "config.h"
#include <json/json.h>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../arghelpers.hpp"



/**
 *  openvpn3 config-dump command
 *
 *  This command is used to show the contents of a configuration profile.
 *  It allows both the textual representation, which is compatible with
 *  OpenVPN 2.x based clients as well as JSON by providing the --json option.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_dump(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-dump",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        std::string path = (args->Present("config")
                                ? retrieve_config_path("config-dump",
                                                       args->GetValue("config", 0))
                                : args->GetValue("path", 0));
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path, true);
        if (!conf.CheckObjectExists())
        {
            throw CommandException("config-dump",
                                   "Configuration does not exist");
        }


        if (!args->Present("json"))
        {
            std::cout << conf.GetConfig() << std::endl;
        }
        else
        {
            std::stringstream cfgstr(conf.GetJSONConfig());
            Json::Value cfgjson;
            cfgstr >> cfgjson;
            Json::Value json;

            json["object_path"] = path;
            json["owner"] = (uint32_t)conf.GetOwner();
            json["name"] = conf.GetStringProperty("name");

            if (conf.CheckFeatures(CfgMgrFeatures::TAGS))
            {
                for (const auto &tag : conf.GetTags())
                {
                    json["tags"].append(tag);
                }
            }

            json["import_timestamp"] = (Json::Value::UInt64)conf.GetUInt64Property("import_timestamp");
            json["last_used_timestamp"] = (Json::Value::UInt64)conf.GetUInt64Property("last_used_timestamp");
            json["locked_down"] = conf.GetLockedDown();
            json["transfer_owner_session"] = conf.GetTransferOwnerSession();
            json["readonly"] = conf.GetBoolProperty("readonly");
            json["used_count"] = conf.GetUIntProperty("used_count");
            json["profile"].append(cfgjson);
            json["dco"] = conf.GetDCO();

            json["public_access"] = conf.GetPublicAccess();
            for (const auto &e : conf.GetAccessList())
            {
                json["acl"].append(e);
            }
            for (const auto &ov : conf.GetOverrides())
            {
                switch (ov.override.type)
                {
                case OverrideType::boolean:
                    json["overrides"][ov.override.key] = ov.boolValue;
                    break;

                case OverrideType::string:
                    json["overrides"][ov.override.key] = ov.strValue;
                    break;

                default:
                    throw CommandException("config-dump",
                                           "Invalid override type for key "
                                           "'" + ov.override.key
                                               + "'");
                }
            }

            std::cout << json << std::endl;
        }
        return 0;
    }
    catch (DBusException &err)
    {
        throw CommandException("config-dump", err.GetRawError());
    }
}


/**
 *  Creates the SingleCommand object for the 'config-dump' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_dump()
{
    //
    //  config-show command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-dump",
                                "Show/dump a configuration profile",
                                cmd_config_dump));
    cmd->SetAliasCommand("config-show",
                         "**\n** This is command deprecated, use config-dump instead\n**");
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "OBJ-PATH",
                                   true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("json",
                   'j',
                   "Dump the configuration in JSON format");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 config-remove command
 *
 *  This command will delete and remove a configuration profile from the
 *  Configuration Manager
 *
 * @param args  ParsedArgs object containing Valid related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_remove(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-remove",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        std::string path = (args->Present("config")
                                ? retrieve_config_path("config-remove",
                                                       args->GetValue("config", 0))
                                : args->GetValue("path", 0));

        std::string response;
        if (!args->Present("force"))
        {
            std::cout << "This operation CANNOT be undone and removes this "
                      << "configuration profile completely."
                      << std::endl;
            std::cout << "Are you sure you want to do this? "
                      << "(enter yes in upper case) ";

            std::cin >> response;
        }

        if ("YES" == response || args->Present("force"))
        {
            OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);
            if (!conf.CheckObjectExists())
            {
                throw CommandException("config-remove",
                                       "Configuration does not exist");
            }
            conf.Remove();
            std::cout << "Configuration removed." << std::endl;
        }
        else
        {
            std::cout << "Configuration profile delete operating cancelled"
                      << std::endl;
        }
        return 0;
    }
    catch (DBusException &err)
    {
        throw CommandException("config-remove", err.GetRawError());
    }
    catch (...)
    {
        throw;
    }
}



/**
 *  Creates the SingleCommand object for the 'config-remove' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_remove()
{
    //
    //  config-remove command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-remove",
                                "Remove an available configuration profile",
                                cmd_config_remove));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "OBJ-PATH",
                                   true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("force",
                   "Force the deletion process without asking for confirmation");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
