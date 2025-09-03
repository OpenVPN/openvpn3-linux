//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session-acl.cpp
 *
 * @brief  Manages the Access Control List for a specific running session
 *
 */

#include <string>
#include <gdbuspp/connection.hpp>

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../../arghelpers.hpp"


/**
 *  openvpn3 session-acl command
 *
 *  Command to modify the access control to a specific VPN session.
 *  All operations related to granting, revoking, public-access are handled
 *  by this command.
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
static int cmd_session_acl(ParsedArgs::Ptr args)
{
    int ret = 0;
    if (!args->Present("path") && !args->Present("config")
        && !args->Present("interface"))
    {
        throw CommandException("session-acl",
                               "Missing required session path or config name");
    }

    if (!args->Present("show")
        && !args->Present("grant")
        && !args->Present("revoke")
        && !args->Present("public-access")
        && !args->Present("allow-log-access")
        && !args->Present("lock-down")
        && !args->Present("seal"))
    {
        throw CommandException("session-acl", "No operation option provided");
    }

    try
    {
        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

        std::string sesspath = "";
        if (args->Present("config"))
        {
            auto paths = sessmgr->LookupConfigName(args->GetValue("config", 0));
            if (0 == paths.size())
            {
                throw CommandException("session-acl",
                                       "No sessions started with the "
                                       "configuration profile name was found");
            }
            else if (1 < paths.size())
            {
                throw CommandException("session-acl",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            sesspath = paths.at(0);
        }
        else if (args->Present("interface"))
        {
            sesspath = sessmgr->LookupInterface(args->GetValue("interface", 0));
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        auto session = sessmgr->Retrieve(sesspath);
        if (!session->CheckSessionExists())
        {
            throw CommandException("session-acl",
                                   "Session not found");
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
                        session->AccessGrant(uid);
                        std::cout << "Granted access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (const DBus::Exception &)
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
                        session->AccessRevoke(uid);
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
        if (args->Present("public-access"))
        {
            std::string ld = args->GetValue("public-access", 0);
            if (("false" == ld) || ("true" == ld))
            {
                session->SetPublicAccess("true" == ld);
                if ("true" == ld)
                {
                    std::cout << "Session is now accessible by everyone"
                              << std::endl;
                }
                else
                {
                    std::cout << "Session is now only accessible to "
                              << "specific users"
                              << std::endl;
                }
            }
            else
            {
                std::cerr << "** ERROR ** --public-access argument must be "
                          << "'true' or 'false'"
                          << std::endl;
                ret = 3;
            }
        }

        if (args->Present("allow-log-access"))
        {
            bool ala = args->GetBoolValue("allow-log-access", 0);
            session->SetRestrictLogAccess(!ala);
            if (ala)
            {
                std::cout << "Session log is now accessible to users granted "
                          << " session access" << std::endl;
            }
            else
            {
                std::cout << "Session log is only accessible the session"
                          << " owner" << std::endl;
            }
        }


        if (args->Present("show"))
        {
            std::string owner = lookup_username(session->GetOwner());
            std::cout << "                    Owner: ("
                      << session->GetOwner() << ") "
                      << " " << ('(' != owner[0] ? owner : "(unknown)")
                      << std::endl;

            std::cout << "            Public access: "
                      << (session->GetPublicAccess() ? "yes" : "no")
                      << std::endl;

            std::cout << " Users granted log access: "
                      << (session->GetRestrictLogAccess() ? "no" : "yes")
                      << std::endl;

            if (!session->GetPublicAccess())
            {
                std::vector<uid_t> acl = session->GetAccessList();
                std::cout << "     Users granted access: " << std::to_string(acl.size())
                          << (1 != acl.size() ? " users" : " user")
                          << std::endl;
                for (auto const &uid : acl)
                {
                    std::string user = lookup_username(uid);
                    std::cout << "                           - (" << uid << ") "
                              << " " << ('(' != user[0] ? user : "(unknown)")
                              << std::endl;
                }
            }
        }
        return ret;
    }
    catch (const DBus::Exception &excp)
    {
        throw CommandException("session-acl", excp.GetRawError());
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  Creates the SingleCommand object for the 'session-acl' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_acl()
{
    //
    //  session-acl command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-acl",
                                "Manage access control lists for sessions",
                                cmd_session_acl));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "SESSION-PATH",
                                   true,
                                   "Path to the session in the session "
                                   "manager",
                                   arghelper_session_paths);
    path_opt->SetAlias("session-path");
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names_sessions);
    cmd->AddOption("interface",
                   'I',
                   "INTERFACE",
                   true,
                   "Alternative to --path, where tun interface name is used "
                   "instead",
                   arghelper_managed_interfaces);
    cmd->AddOption("show",
                   's',
                   "Show the current access control lists");
    cmd->AddOption("grant",
                   'G',
                   "<UID | username>",
                   true,
                   "Grant this user access to this session");
    cmd->AddOption("revoke",
                   'R',
                   "<UID | username>",
                   true,
                   "Revoke this user access from this session");
    cmd->AddOption("public-access",
                   "<true|false>",
                   true,
                   "Set/unset the public access flag",
                   arghelper_boolean);
    cmd->AddOption("allow-log-access",
                   "<true|false>",
                   true,
                   "Can users granted access also access the session log?",
                   arghelper_boolean);

    return cmd;
}
