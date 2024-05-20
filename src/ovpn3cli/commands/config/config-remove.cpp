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
 * @file   config/config-remove.hpp
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

    DBus::Connection::Ptr dbuscon = nullptr;
    OpenVPN3ConfigurationProxy::Ptr conf = nullptr;
    try
    {
        dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        std::string path = (args->Present("config")
                                ? retrieve_config_path("config-remove",
                                                       args->GetValue("config", 0),
                                                       dbuscon)
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
            conf = OpenVPN3ConfigurationProxy::Create(dbuscon, path);
            if (!conf->CheckObjectExists())
            {
                throw CommandException("config-remove",
                                       "Configuration does not exist");
            }
            conf->Remove();
            std::cout << "Configuration removed." << std::endl;
        }
        else
        {
            std::cout << "Configuration profile delete operating cancelled"
                      << std::endl;
        }
        return 0;
    }
    catch (const DBus::Exception &err)
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
