//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session.hpp
 *
 * @brief  Commands to start and manage VPN sessions
 */

#include <csignal>
#include <json/json.h>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "common/open-uri.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../arghelpers.hpp"



//////////////////////////////////////////////////////////////////////////


// Implemented in config-import.cpp
std::string import_config(const std::string &filename,
                          const std::string &cfgname,
                          const bool single_use,
                          const bool persistent,
                          const std::vector<std::string> &tags);

/**
 *  openvpn3 session-start command
 *
 *  This command is used to initate and start a new VPN session
 *
 * @param args
 * @return
 */
static int cmd_session_start(ParsedArgs::Ptr args)
{
    if (!args->Present("config-path") && !args->Present("config"))
    {
        throw CommandException("session-start",
                               "Either --config or --config-path must be provided");
    }
    if (args->Present("config-path") && args->Present("config"))
    {
        throw CommandException("session-start",
                               "--config and --config-path cannot be used together");
    }
    if (args->Present("persist-tun") && !args->Present("config"))
    {
        throw CommandException("session-start",
                               "--persist-tun can only be used with --config");
    }

    int timeout = -1;
    if (args->Present("timeout"))
    {
        timeout = std::atoi(args->GetValue("timeout", 0).c_str());
    }

    try
    {
        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        std::string cfgpath;
        if (args->Present("config"))
        {
            // This will first lookup the configuration name in the
            // Configuration Manager before attempting to load a local file.
            // If multiple configurations carry the same configuration name,
            // it will fail as we do not support handling this scenario.  It

            std::string cfgname = args->GetValue("config", 0);
            try
            {
                cfgpath = retrieve_config_path("session-start", cfgname);
                std::cout << "Using pre-loaded configuration profile '"
                          << cfgname << "'" << std::endl;
            }
            catch (const CommandException &excp)
            {
                std::string err(excp.what());
                if (err.find("More than one configuration profile was found") != std::string::npos)
                {
                    throw;
                }
                cfgpath = import_config(cfgname, cfgname, true, false, {});
                std::cout << "Using configuration profile from file: "
                          << cfgname << std::endl;
            }
        }
        else
        {
            cfgpath = args->GetValue("config-path", 0);
        }

        // If --persist-tun is given on the command line, enforce this
        // feature on this connection.  This can only be provided when using
        // --config, not --config-path.
        if (args->Present("persist-tun"))
        {
            OpenVPN3ConfigurationProxy cfgprx(G_BUS_TYPE_SYSTEM, cfgpath);
            const ValidOverride &vo = cfgprx.LookupOverride("persist-tun");
            cfgprx.SetOverride(vo, true);
        }

        // Create a new tunnel session
        OpenVPN3SessionProxy::Ptr session = sessmgr.NewTunnel(cfgpath);

        std::cout << "Session path: " << session->GetPath() << std::endl;

#ifdef ENABLE_OVPNDCO
        if (args->Present("dco"))
        {
            // If a certain DCO state was requested, set the DCO flag before
            // starting the connection
            session->SetDCO(args->GetBoolValue("dco", false));
        }
#endif

        start_session(session, SessionStartMode::START, timeout, args->Present("background"));
        return 0;
    }
    catch (const SessionException &excp)
    {
        std::string err{excp.what()};
        if (err.find("ERR_PROFILE_SERVER_LOCKED_UNSUPPORTED:") != std::string::npos)
        {
            throw CommandException("session-start", "Server locked profiles are unsupported");
        }
        throw CommandException("session-start", excp.what());
    }
    catch (...)
    {
        throw;
    }
}

/**
 *  Creates the SingleCommand object for the 'session-start' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_start()
{
    //
    //  session-start command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-start",
                                "Start a new VPN session",
                                cmd_session_start));
    cmd->AddOption("config",
                   'c',
                   "CONFIG-FILE",
                   true,
                   "Configuration file to start directly",
                   arghelper_config_names);
    cmd->AddOption("config-path",
                   'p',
                   "CONFIG-PATH",
                   true,
                   "Configuration path to an already imported configuration",
                   arghelper_config_paths);
    cmd->AddOption("persist-tun",
                   0,
                   "Enforces persistent tun/seamless tunnel (requires --config)");
    cmd->AddOption("timeout",
                   0,
                   "SECS",
                   true,
                   "Connection attempt timeout (default: infinite)");
    cmd->AddOption("background",
                   0,
                   "Starts the connection in the background after basic credentials are provided");
#ifdef ENABLE_OVPNDCO
    cmd->AddOption("dco",
                   0,
                   "BOOL",
                   true,
                   "Start the connection using Data Channel Offload kernel acceleration",
                   arghelper_boolean);
#endif

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 session-manage command
 *
 *  This command handles pausing, resuming and disconnecting established
 *  sessions.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 */
static int cmd_session_manage(ParsedArgs::Ptr args)
{
    // clang-format off
    const unsigned int mode_pause      = 1 << 0;
    const unsigned int mode_resume     = 1 << 1;
    const unsigned int mode_restart    = 1 << 2;
    const unsigned int mode_disconnect = 1 << 3;
    const unsigned int mode_cleanup    = 1 << 4;
    const unsigned int mode_log_level  = 1 << 5;
    // clang-format on

    unsigned int mode = 0;
    unsigned int mode_count = 0;
    if (args->Present("pause"))
    {
        mode |= mode_pause;
        mode_count++;
    }
    if (args->Present("resume"))
    {
        mode |= mode_resume;
        mode_count++;
    }
    if (args->Present("restart"))
    {
        mode |= mode_restart;
        mode_count++;
    }
    if (args->Present("disconnect"))
    {
        mode |= mode_disconnect;
        mode_count++;
    }
    if (args->Present("cleanup"))
    {
        mode |= mode_cleanup;
        mode_count++;
    }

    if (args->Present("log-level"))
    {
        mode |= mode_log_level;
        mode_count++;
    }

    if (0 == mode_count)
    {
        throw CommandException("session-manage",
                               "One of --pause, --resume, --restart, "
                               "--disconnect, --cleanup or --log-level "
                               "must be present");
    }
    if (1 < mode_count)
    {
        throw CommandException("session-manage",
                               "--pause, --resume, --restart, --disconnect "
                               "--cleanup or --log-level "
                               "cannot be used together");
    }

    // Only --cleanup does NOT depend on --path or --config
    if (!args->Present("path") && !args->Present("config")
        && !args->Present("interface") && (mode ^ mode_cleanup) > 0)
    {
        throw CommandException("session-manage",
                               "Missing required session path or config name");
    }


    try
    {
        int timeout = -1;
        if (args->Present("timeout"))
        {
            timeout = std::atoi(args->GetValue("timeout", 0).c_str());
        }

        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        if (mode_cleanup == mode)
        {
            // Loop through all open sessions and check if they have a valid
            // status available.  A valid status means it is not empty nor
            // unset.  If the status can't be retrieved, it is also
            // invalid.
            std::vector<OpenVPN3SessionProxy::Ptr> sessions = sessmgr.FetchAvailableSessions();
            std::cout << "Cleaning up stale sessions - Found "
                      << std::to_string(sessions.size()) << " open sessions "
                      << "to check" << std::endl;

            unsigned int c = 0;
            for (const auto &s : sessions)
            {
                try
                {
                    std::string cfgname = s->GetStringProperty("config_name");

                    std::cout << "Checking:  " << cfgname << " - " << s->GetPath()
                              << " ... ";

                    StatusEvent st = s->GetLastStatus();
                    if (st.Check(StatusMajor::UNSET, StatusMinor::UNSET)
                        && st.message.empty())
                    {
                        // This is an empty and unset status
                        // These are rare, as it means the
                        // openvpn3-service-client process is running and
                        // responsive, but has not even managed to load a
                        // configuration profile
                        s->Disconnect();
                        std::cout << "Removed" << std::endl;
                        ++c;
                    }
                    else
                    {
                        std::cout << "Valid, keeping it" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    // Errors in this case indicates we cannot retrieve any
                    // information about the session; thus it is most likely
                    // not valid any more, so we kill it - and ignore any
                    // errors while killing it.
                    //
                    // When this happens the openvpn3-service-client process
                    // is most likely already dead.  Calling the Disconnect()
                    // method basically just cleans up the session in the
                    // Session Manager only.
                    try
                    {
                        s->Disconnect();
                    }
                    catch (...)
                    {
                    }
                    std::cout << "Removed" << std::endl;
                    ++c;
                }
            }
            std::cout << std::to_string(c) << " session" << (c != 1 ? "s" : "")
                      << " removed" << std::endl;
            return 0;
        }


        std::string sesspath = "";
        if (args->Present("config"))
        {
            std::vector<std::string> paths = sessmgr.LookupConfigName(args->GetValue("config", 0));
            if (0 == paths.size())
            {
                throw CommandException("session-manage",
                                       "No sessions started with the "
                                       "configuration profile name was found");
            }
            else if (1 < paths.size())
            {
                throw CommandException("session-manage",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            sesspath = paths.at(0);
        }
        else if (args->Present("interface"))
        {
            sesspath = sessmgr.LookupInterface(args->GetValue("interface", 0));
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        OpenVPN3SessionProxy::Ptr session;
        session.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, sesspath));

        if (!session->CheckObjectExists())
        {
            throw CommandException("session-manage",
                                   "Session not found");
        }

        ConnectionStats stats;

        switch (mode)
        {
        case mode_pause:
            session->Pause("Front-end request");
            std::cout << "Initiated session pause: " << sesspath
                      << std::endl;
            return 0;

        case mode_resume:
            std::cout << "Resuming session: " << sesspath
                      << std::endl;
            start_session(session, SessionStartMode::RESUME, timeout, timeout < 0);
            return 0;

        case mode_restart:
            std::cout << "Restarting session: " << sesspath
                      << std::endl;
            start_session(session, SessionStartMode::RESTART, timeout, timeout < 0);
            return 0;

        case mode_disconnect:
            try
            {
                stats = session->GetConnectionStats();
            }
            catch (...)
            {
                std::cout << "Connection statistics is not available"
                          << std::endl;
            }
            session->Disconnect();
            std::cout << "Initiated session shutdown." << std::endl;
            std::cout << statistics_plain(stats);
            break;

        case mode_log_level:
            unsigned int cur = session->GetLogVerbosity();
            std::cout << "Current log level: " << std::to_string(cur) << std::endl;

            if (args->GetValueLen("log-level") > 0)
            {
                unsigned int verb = std::atoi(args->GetLastValue("log-level").c_str());
                if (verb != cur)
                {
                    session->SetLogVerbosity(verb);
                    std::cout << "New log level: " << std::to_string(verb)
                              << std::endl;
                }
            }
            break;
        }
        return 0;
    }
    catch (const DBusException &excp)
    {
        throw CommandException("session-manage", excp.GetRawError());
    }
    catch (const SessionException &excp)
    {
        throw CommandException("session-manage", excp.what());
    }
    catch (...)
    {
        throw;
    }
}

/**
 *  Creates the SingleCommand object for the 'session-manage' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_manage()
{
    //
    //  session-manage command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-manage",
                                "Manage VPN sessions",
                                cmd_session_manage));
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
    cmd->AddOption("log-level",
                   0,
                   "LEVEL",
                   false,
                   "View/Set the log-level for a running VPN session",
                   arghelper_log_levels);
    cmd->AddOption("timeout",
                   0,
                   "SECS",
                   true,
                   "Connection attempt timeout for resume and restart "

                   "(default: infinite)");
    cmd->AddOption("pause",
                   'P',
                   "Pauses the VPN session");
    cmd->AddOption("resume",
                   'R',
                   "Resumes a paused VPN session");
    cmd->AddOption("restart",
                   "Disconnect and reconnect a running VPN session");
    cmd->AddOption("disconnect",
                   'D',
                   "Disconnects a VPN session");
    cmd->AddOption("cleanup",
                   0,
                   "Clean up stale sessions");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
