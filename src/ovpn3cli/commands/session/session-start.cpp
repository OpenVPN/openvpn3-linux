//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session-start.cpp
 *
 * @brief  Command to start new VPN sessions
 */

#include <string>
#include <gdbuspp/connection.hpp>

#include "helpers.hpp"
#include "common/cmdargparser.hpp"
#include "events/status.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "helpers.hpp"
#include "../../arghelpers.hpp"


using namespace ovpn3cli::session;


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
        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

        DBus::Object::Path cfgpath{};
        if (args->Present("config"))
        {
            // This will first lookup the configuration name in the
            // Configuration Manager before attempting to load a local file.
            // If multiple configurations carry the same configuration name,
            // it will fail as we do not support handling this scenario.  It

            std::string cfgname = args->GetValue("config", 0);
            try
            {
                cfgpath = retrieve_config_path("session-start",
                                               cfgname,
                                               dbuscon);
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
                cfgpath = import_config(dbuscon, cfgname, cfgname, true, false, {});
                std::cout << "Using configuration profile from file: "
                          << cfgname << std::endl;
            }
        }
        else
        {
            cfgpath = args->GetValue("config-path", 0);
        }

        auto cfgprx = OpenVPN3ConfigurationProxy::Create(dbuscon, cfgpath, true);
        cfgprx->Validate();

        // If --persist-tun is given on the command line, enforce this
        // feature on this connection.  This can only be provided when using
        // --config, not --config-path.
        if (args->Present("persist-tun"))
        {
            const ValidOverride &vo = cfgprx->LookupOverride("persist-tun");
            cfgprx->SetOverride(vo, true);
        }

        // Create a new tunnel session
        auto session = sessmgr->NewTunnel(cfgpath);
        std::cout << "Session path: " << session->GetPath() << std::endl;

#ifdef ENABLE_OVPNDCO
        if (args->Present("dco"))
        {
            // If a certain DCO state was requested, set the DCO flag before
            // starting the connection
            session->SetDCO(args->GetBoolValue("dco", false));
        }
#endif

        start_session(session,
                      SessionStartMode::START,
                      timeout,
                      args->Present("background"));
        return 0;
    }
    catch (const CfgMgrProxyException &excp)
    {
        throw CommandException("session-start", excp.GetRawError());
    }
    catch (const SessionException &excp)
    {
        if (!excp.GetDetails().empty())
        {
            std::cout << "Could not start session: "
                      << excp.GetDetails() << std::endl;
        }
        throw CommandException("session-start", excp.what());
    }
    catch (const DBus::Exception &excp)
    {
        throw CommandException("session-start",
                               "Could not start new VPN session: "
                                   + std::string(excp.GetRawError()));
    }
    catch (const std::exception &excp)
    {
        std::cerr << "---- " << excp.what() << std::endl;
        return 3;
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
