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
 * @file   config/config-dump.hpp
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
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../../arghelpers.hpp"


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

    DBus::Connection::Ptr dbuscon = nullptr;
    OpenVPN3ConfigurationProxy::Ptr conf = nullptr;
    try
    {
        dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        std::string path = (args->Present("config")
                                ? retrieve_config_path("config-dump",
                                                       args->GetValue("config", 0),
                                                       dbuscon)
                                : args->GetValue("path", 0));
        conf = OpenVPN3ConfigurationProxy::Create(dbuscon, path);
        if (!conf->CheckObjectExists())
        {
            throw CommandException("config-dump",
                                   "Configuration does not exist");
        }


        if (!args->Present("json"))
        {
            std::cout << conf->GetConfig() << std::endl;
        }
        else
        {
            std::stringstream cfgstr(conf->GetJSONConfig());
            Json::Value cfgjson;
            cfgstr >> cfgjson;
            Json::Value json;

            json["object_path"] = path;
            json["owner"] = (uint32_t)conf->GetOwner();
            json["name"] = conf->GetName();

            if (conf->CheckFeatures(CfgMgrFeatures::TAGS))
            {
                for (const auto &tag : conf->GetTags())
                {
                    json["tags"].append(tag);
                }
            }

            json["import_timestamp"] = (Json::Value::UInt64)conf->GetImportTimestamp();
            json["last_used_timestamp"] = (Json::Value::UInt64)conf->GetLastUsedTimestamp();
            json["locked_down"] = conf->GetLockedDown();
            json["transfer_owner_session"] = conf->GetTransferOwnerSession();
            json["readonly"] = conf->GetSealed();
            json["used_count"] = conf->GetUsedCounter();
            json["profile"].append(cfgjson);
            json["dco"] = conf->GetDCO();

            json["public_access"] = conf->GetPublicAccess();
            for (const auto &e : conf->GetAccessList())
            {
                json["acl"].append(e);
            }
            for (const auto &ov : conf->GetOverrides())
            {
                if (std::holds_alternative<std::string>(ov.value))
                {
                    json["overrides"][ov.key] = std::get<std::string>(ov.value);
                }
                else
                {
                    json["overrides"][ov.key] = std::get<bool>(ov.value);
                }
            }

            std::cout << json << std::endl;
        }
        return 0;
    }
    catch (const DBus::Exception &err)
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
    //  config-dump command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-dump",
                                "Show/dump a configuration profile",
                                cmd_config_dump));

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
