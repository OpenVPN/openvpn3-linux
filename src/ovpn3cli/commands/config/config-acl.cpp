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
 * @file   config/config-acl.hpp
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
 *  openvpn3 config-acl command
 *
 *  Command to modify the access control to a specific configuration profile.
 *  All operations related to granting, revoking, public-access, locking-down
 *  and sealing (making configurations read-only) are handled by this command.
 *
 *  Also note that you can run multiple operations in a single command line.
 *  It is fully possible to use --grant, --revoke and --show in a single
 *  command line.  In addition, both --grant and --revoke can be used multiple
 *  times to grant/revoke several users in a single operation.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_acl(ParsedArgs::Ptr args)
{
    int ret = 0;
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-acl",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        args->Present({"show",
                       "grant",
                       "revoke",
                       "public-access",
                       "lock-down",
                       "seal",
                       "transfer-owner-session"});
    }
    catch (const OptionNotFound &)
    {
        throw CommandException("config-acl", "No operation option provided");
    }

    DBus::Connection::Ptr dbuscon = nullptr;
    OpenVPN3ConfigurationProxy::Ptr conf = nullptr;
    try
    {
        dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        std::string path = (args->Present("config")
                                ? retrieve_config_path("config-acl",
                                                       args->GetValue("config", 0),
                                                       dbuscon)
                                : args->GetValue("path", 0));

        conf = OpenVPN3ConfigurationProxy::Create(dbuscon, path);
        if (!conf->CheckObjectExists())
        {
            throw CommandException("config-acl",
                                   "Configuration does not exist");
        }

        if (args->Present("grant"))
        {
            for (auto const &user : args->GetAllValues("grant"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        conf->AccessGrant(uid);
                        std::cout << "Granted access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (const DBus::Exception &e)
                    {
                        std::cerr << "Failed granting access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException &)
                {
                    std::cerr << "** ERROR ** --grant " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }

        if (args->Present("revoke"))
        {
            for (auto const &user : args->GetAllValues("revoke"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        conf->AccessRevoke(uid);
                        std::cout << "Revoked access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (const DBus::Exception &e)
                    {
                        std::cerr << "Failed revoking access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException &)
                {
                    std::cerr << "** ERROR ** --revoke " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }

        if (args->Present("transfer-owner-session"))
        {
            bool v = args->GetBoolValue("transfer-owner-session", 0);
            conf->SetTransferOwnerSession(v);
            std::cout << "Configuration ownership transfer to session "
                      << (v ? "enabled" : "disabled")
                      << std::endl;
        }

        if (args->Present("lock-down"))
        {
            bool ld = args->GetBoolValue("lock-down", 0);
            conf->SetLockedDown(ld);
            if (ld)
            {
                std::cout << "Configuration has been locked down"
                          << std::endl;
            }
            else
            {
                std::cout << "Configuration has been opened up"
                          << std::endl;
            }
        }


        if (args->Present("public-access"))
        {
            bool ld = args->GetBoolValue("public-access", 0);
            conf->SetPublicAccess(ld);
            if (ld)
            {
                std::cout << "Configuration is now readable to everyone"
                          << std::endl;
            }
            else
            {
                std::cout << "Configuration is now readable to only "
                          << "specific users"
                          << std::endl;
            }
        }

        if (args->Present("seal"))
        {
            std::cout << "This operation CANNOT be undone and makes this "
                      << "configuration profile read-only."
                      << std::endl;
            std::cout << "Are you sure you want to do this? "
                      << "(enter yes in upper case) ";

            std::string response;
            std::cin >> response;
            if ("YES" == response)
            {
                conf->Seal();
                std::cout << "Configuration has been sealed." << std::endl;
            }
            else
            {
                std::cout << "--seal operation has been cancelled"
                          << std::endl;
            }
        }

        if (args->Present("show"))
        {
            std::cout << std::endl
                      << "    Configuration path: "
                      << conf->GetConfigPath() << std::endl
                      << "                  Name: "
                      << conf->GetName()
                      << std::endl;

            std::string owner = lookup_username(conf->GetOwner());
            std::cout << "                 Owner: ("
                      << conf->GetOwner() << ") "
                      << " " << ('(' != owner[0] ? owner : "(unknown)")
                      << std::endl;

            std::cout << "             Read-only: "
                      << (conf->GetSealed() ? "yes" : "no")
                      << std::endl;

            std::cout << "           Locked down: "
                      << (conf->GetLockedDown() ? "yes" : "no")
                      << std::endl;

            std::cout << "    Ownership transfer: "
                      << (conf->GetTransferOwnerSession() ? "yes" : "no")
                      << std::endl;

            std::cout << "         Public access: "
                      << (conf->GetPublicAccess() ? "yes" : "no")
                      << std::endl;

            if (!conf->GetPublicAccess())
            {
                std::vector<uid_t> acl = conf->GetAccessList();
                std::cout << "  Users granted access: " << std::to_string(acl.size())
                          << (1 != acl.size() ? " users" : " user")
                          << std::endl;
                for (auto const &uid : acl)
                {
                    std::string user = lookup_username(uid);
                    std::cout << "                        - (" << uid << ") "
                              << " " << ('(' != user[0] ? user : "(unknown)")
                              << std::endl;
                }
            }
            std::cout << std::endl;
        }
        return ret;
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  Creates the SingleCommand object for the 'config-acl' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_acl()
{
    //
    //  config-acl command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-acl",
                                "Manage access control lists for configurations",
                                cmd_config_acl));
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
    cmd->AddOption("show",
                   's',
                   "Show the current access control lists");
    cmd->AddOption("grant",
                   'G',
                   "<UID | username>",
                   true,
                   "Grant this user access to this configuration profile");
    cmd->AddOption("revoke",
                   'R',
                   "<UID | username>",
                   true,
                   "Revoke this user access from this configuration profile");
    cmd->AddOption("public-access",
                   "<true|false>",
                   true,
                   "Set/unset the public access flag",
                   arghelper_boolean);
    cmd->AddOption("transfer-owner-session",
                   'T',
                   "<true|false>",
                   true,
                   "Transfer the configuration ownership to the VPN session at startup",
                   arghelper_boolean);
    cmd->AddOption("lock-down",
                   "<true|false>",
                   true,
                   "Set/unset the lock-down flag.  Will disable config retrieval for users",
                   arghelper_boolean);
    cmd->AddOption("seal",
                   'S',
                   "Make the configuration profile permanently read-only");

    return cmd;
}
