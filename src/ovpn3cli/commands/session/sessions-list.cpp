//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessions-list.cpp
 *
 * @brief  Lists all started and running VPN sessions for the current user
 */

#include <string>
#include <gdbuspp/connection.hpp>

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "events/status.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"


/**
 *  openvpn3 sessions-list command
 *
 *  Lists all available VPN sessions.  Only sessions where the
 *  calling user is the owner, have been added to the access control list
 *  or sessions tagged with public_access will be listed.  This restriction
 *  is handled by the Session Manager
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 */
static int cmd_sessions_list(ParsedArgs::Ptr args)
{
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

    bool first = true;
    for (auto &sprx : sessmgr->FetchAvailableSessions())
    {
        // Retrieve the name of the configuration profile used
        std::stringstream config_line;
        bool config_deleted = false;
        try
        {
            // Retrieve the current configuration profile name from the
            // configuration manager
            std::string cfgname_current = "";
            std::string config_path = sprx->GetConfigPath();
            try
            {
                auto cprx = OpenVPN3ConfigurationProxy::Create(dbuscon, config_path);
                if (cprx->CheckObjectExists())
                {
                    cfgname_current = cprx->GetName();
                }
                else
                {
                    config_deleted = true;
                }
            }
            catch (...)
            {
                // Failure is okay here, the profile may be deleted.
                config_deleted = true;
            }

            // Retrieve the configuration profile name used when starting
            // the VPN sessions
            std::string cfgname = sprx->GetConfigName();
            if (!cfgname.empty())
            {
                config_line << " Config name: " << cfgname;
                if (config_deleted)
                {
                    config_line << "  (Config not available)";
                }
                else if (cfgname_current != cfgname)
                {
                    config_line << "  (Current name: "
                                << cfgname_current << ")";
                }
                config_line << std::endl;
            }
        }
        catch (const DBus::Exception &excp)
        {
            // The session has not been initialised properly yet, so
            // configuration name is not available
            config_line << "";
        }


        // Retrieve the session start timestamp
        std::string created{};
        try
        {
            created = sprx->GetSessionCreated();
        }
        catch (const DBus::Exception &)
        {
            created = "(Not available)";
        }

        // Retrieve session owner information and VPN backend process PID
        std::string owner{};
        pid_t be_pid = -1;
        try
        {
            owner = lookup_username(sprx->GetOwner());
            be_pid = sprx->GetBackendPid();
        }
        catch (const DBus::Exception &)
        {
            owner = "(not available)";
            be_pid = -1;
        }

        // Retrieve the tun interface name for this session
        std::string devname{};
        bool dco = false;
        try
        {
            devname = sprx->GetDeviceName();
#ifdef ENABLE_OVPNDCO
            dco = sprx->GetDCO();
#endif
        }
        catch (const DBus::Exception &)
        {
            // The session may not have been started yet; the error
            // in this case isn't that valuable so we just consider
            // the device name not set.
            devname = "(None)";
        }

        // Retrieve the session name set by the VPN backend
        std::stringstream sessionname_line;
        try
        {
            std::string sessname = sprx->GetSessionName();
            if (!sessname.empty())
            {
                sessionname_line << "Session name: " << sessname << std::endl;
            }
        }
        catch (const DBus::Exception &)
        {
            // Ignore any errors if this property is unavailable
            sessionname_line << "";
        }

        Events::Status status{};
        try
        {
            status = sprx->GetLastStatus();
        }
        catch (const DBus::Exception &)
        {
        }

        // Output separator lines
        if (first)
        {
            std::cout << std::setw(77) << std::setfill('-') << "-" << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }
        first = false;

        // Output session information
        std::cout << "        Path: " << sprx->GetPath() << std::endl;
        std::cout << "     Created: " << created
                  << std::setw(47 - created.size()) << std::setfill(' ')
                  << " PID: "
                  << (be_pid > 0 ? std::to_string(be_pid) : "-")
                  << std::endl;
        std::cout << "       Owner: " << owner
                  << std::setw(47 - owner.size()) << "Device: " << devname
                  << (dco ? " (DCO)" : "")
                  << std::endl;
        std::cout << config_line.str();
        std::cout << sessionname_line.str();

        std::cout << "      Status: ";
        if (status.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            std::cout << "Web authentication required to connect" << std::endl;
        }
        else
        {
            std::cout << status << std::endl;
        }
    }

    // Output closing separator
    if (first)
    {
        std::cout << "No sessions available" << std::endl;
    }
    else
    {
        std::cout << std::setw(77) << std::setfill('-') << "-" << std::endl;
    }

    return 0;
}

/**
 *  Creates the SingleCommand object for the 'session-list' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_sessions_list()
{
    //
    //  sessions-list command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("sessions-list",
                                "List available VPN sessions",
                                cmd_sessions_list));

    return cmd;
}
