//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2021-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2021-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-service.cpp
 *
 * @brief  Command for interacting with the openvpn3-service-sessionmgr
 */

#include "build-config.h"
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>

#ifdef HAVE_TINYXML
#include <openvpn/common/exception.hpp>
#include <openvpn/common/xmlhelper.hpp>

using namespace openvpn;

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../arghelpers.hpp"


static std::vector<std::string> get_running_sessions(DBus::Connection::Ptr dbuscon)
{
    auto smgr = SessionManager::Proxy::Manager::Create(dbuscon);
    const XmlDocPtr introsp = smgr->Introspect();

    tinyxml2::XMLNode *root = introsp->FirstChildElement();
    const tinyxml2::XMLElement *node = Xml::find(root, "node");
    std::vector<std::string> paths{};
    while (nullptr != node)
    {
        const char *name = node->Attribute("name");
        std::string session_path = Constants::GenPath("sessions") + "/"
                                   + std::string(name);
        paths.push_back(session_path);

        node = Xml::next_sibling(node);
    }

    return paths;
}


static void list_running_sessions(bool verbose)
{
    if (::geteuid() != 0)
    {
        // This is a pretty lame way to restrict this for root only.
        // But there will be exceptions thrown on sessions non-root users
        // will not have access too later on, getting a ugly looking listing.
        //
        // This way we avoid complicating the logic below further adding
        // support for something which isn't supposed to be supported.
        //
        // Non-root users can try to introspect the sessionmgr service
        // directly and will not get too much details from there anyhow
        // - except of sessions they're granted access to.  Which is
        // essentially what 'openvpn3 sessions-list' does.
        //
        throw CommandException("sessionmgr-service",
                               "This must be run as root");
    }

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    try
    {
        bool first = true;
        for (const auto &session_path : get_running_sessions(dbuscon))
        {
            auto smgr = SessionManager::Proxy::Manager::Create(dbuscon);
            auto sess = smgr->Retrieve(session_path);

            // Retrieve the session start timestamp
            std::string created = sess->GetSessionCreated();

            // Retrieve session owner information and VPN backend process PID
            std::string owner = lookup_username(sess->GetOwner());
            pid_t be_pid = sess->GetBackendPid();

            // Retrieve the tun interface name for this session
            std::string devname;
            try
            {

                devname = sess->GetDeviceName();
                if (!devname.empty())
                {
                    devname += ": ";
#ifdef ENABLE_OVPNDCO
                    if (sess->GetDCO())
                    {
                        devname += "[DCO] ";
                    }
#endif
                }
                else
                {
                    devname = " - ";
                }
            }
            catch (DBus::Exception &)
            {
                // The session may not have been started yet; the error
                // in this case isn't that valuable so we just consider
                // the device name not set.
                devname = "-";
            }

            std::string sessionname{};
            std::string configname{};
            Events::Status status;
            if (verbose)
            {
                // Retrieve the session name set by the VPN backend
                try
                {
                    std::string sessname = sess->GetSessionName();
                    if (!sessname.empty())
                    {
                        sessionname = sessname;
                    }
                }
                catch (DBus::Exception &)
                {
                    // Ignore any errors if this property is unavailable
                    sessionname = "-";
                }

                // Retrieve the configuration profile name used when starting
                // the VPN sessions
                configname = sess->GetConfigName();
                if (configname.empty())
                {
                    configname = "-";
                }

                try
                {
                    status = sess->GetLastStatus();
                }
                catch (DBus::Exception &)
                {
                }
            }


            // Output separator lines
            if (first)
            {
                if (verbose)
                {
                    std::cout << "Device: " << std::setw(10) << " "
                              << "Session path" << std::endl;
                    std::cout << std::setw(18) << " ";
                }
                else
                {
                    std::cout << "Device " << std::setw(11) << " ";
                }
                std::cout << "Created" << std::setw(21) << " "
                          << "Owner" << std::setw(16) << " "
                          << "PID"
                          << std::endl;
                if (verbose)
                {
                    std::cout << std::setw(18) << " "
                              << "Config name" << std::setw(17) << " "
                              << "Session name" << std::setw(28) << " "
                              << std::endl
                              << std::setw(18) << " "
                              << "Last status"
                              << std::endl;
                }
                std::cout << std::setw(79) << std::setfill('-') << "-"
                          << std::setfill(' ') << std::endl;
            }
            else
            {
                if (verbose)
                {
                    std::cout << std::endl;
                }
            }
            first = false;

            if (verbose)
            {
                std::cout << devname << std::setw(18 - devname.length()) << " "
                          << sess->GetPath() << std::endl;
                std::cout << std::setw(18) << " ";
            }
            else
            {
                std::cout << devname << std::setw(18 - devname.length()) << " ";
            }
            std::cout << created << std::setw(28 - created.length()) << " "
                      << owner << std::setw(21 - owner.length()) << " "
                      << (be_pid > 0 ? std::to_string(be_pid) : "-")
                      << std::endl;
            if (verbose)
            {
                std::cout << std::setw(18) << " "
                          << configname << std::setw(28 - configname.length())
                          << " "
                          << sessionname
                          << std::endl;
                std::cout << std::setw(18) << " ";
                if (status.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
                {
                    std::cout << "On-going web authentication";
                }
                else
                {
                    std::cout << status;
                }
                std::cout << std::endl;
            }
        }
        // Output closing separator
        if (first)
        {
            std::cout << "No sessions running" << std::endl;
        }
        else
        {
            std::cout << std::setw(79) << std::setfill('-') << "-" << std::endl;
        }
    }
    catch (const DBus::Exception &excp)
    {
        throw CommandException("sessionmgr-service",
                               excp.GetRawError());
    }
}


/**
 *  openvpn3 sessionmgr-service
 *
 *  This provides an interface to the session manager to retrieve information
 *  otherwise not available via the openvpn3 command.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_sessionmgr_service(ParsedArgs::Ptr args)
{
    if (args->Present("list-sessions"))
    {
        list_running_sessions(false);
    }
    else if (args->Present("list-sessions-verbose"))
    {
        list_running_sessions(true);
    }
    else
    {
        std::cout << "No command operation provided" << std::endl;
    }
    return 0;
}


SingleCommand::Ptr prepare_command_sessionmgr_service()
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("sessionmgr-service",
                                "Interact the OpenVPN 3 Session manager service",
                                cmd_sessionmgr_service));
    cmd->AddOption("list-sessions", 0, "List all running sessions and its owner");
    cmd->AddOption("list-sessions-verbose", 0, "List a more verbose list of running sessions");

    return cmd;
}


#endif // HAVE_TINYXML

//////////////////////////////////////////////////////////////////////////
