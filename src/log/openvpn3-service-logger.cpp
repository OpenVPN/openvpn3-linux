//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2018 OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017-2018 David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>

#define SHUTDOWN_NOTIF_PROCESS_NAME "openvpn3-service-logger"
#include "dbus/core.hpp"
#include "dbus-log.hpp"
#include "common/utils.hpp"
#include "common/cmdargparser.hpp"
#include "logger.hpp"

using namespace openvpn;


static int logger(ParsedArgs args)
{
    int ret = 0;
    bool timestamp = args.Present("timestamp");
    bool colour = args.Present("colour");

    unsigned int log_level = 3;
    if (args.Present("log-level"))
    {
        log_level = std::atoi(args.GetValue("log-level", 0).c_str());
    }

    std::string logfile = "";
    if (args.Present("log-file"))
    {
        logfile = args.GetValue("log-file", 0);
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    Logger * be_subscription = nullptr;
    Logger * session_subscr = nullptr;
    Logger * config_subscr = nullptr;
    try
    {
        if (logfile.empty())
        {
            std::cout << get_version(args.GetArgv0()) << std::endl;
        }

        if (args.Present("vpn-backend"))
        {
            be_subscription = new Logger(dbus.GetConnection(), "[B]",
                                         OpenVPN3DBus_interf_backends,
                                         log_level, timestamp);
            if (!logfile.empty())
            {
                be_subscription->OpenLogFile(logfile);
            }

            if (colour)
            {
                be_subscription->SetColourScheme(Logger::LogColour::BRIGHT_BLUE, Logger::LogColour::BLACK);
            }
        }

        if (args.Present("session-manager")
            || args.Present("session-manager-client-proxy"))
        {
            session_subscr = new Logger(dbus.GetConnection(), "[S]",
                                        OpenVPN3DBus_interf_sessions,
                                        log_level, timestamp);

            if (!logfile.empty())
            {
                session_subscr->OpenLogFile(logfile);
            }

            if (!args.Present("session-manager-client-proxy"))
            {
                // Don't forward log messages from the backend client which
                // are proxied by the session manager.  Unless the
                // --session-manager-client-proxy is used.  These log messages
                // are also available via --vpn-backend, so this is more
                // useful for debug reasons
                session_subscr->AddExcludeFilter(LogGroup::CLIENT);
            }

            if (colour)
            {
                session_subscr->SetColourScheme(
                                Logger::LogColour::BRIGHT_WHITE,
                                Logger::LogColour::BLUE);
            }
        }

        if (args.Present("config-manager"))
        {
            config_subscr = new Logger(dbus.GetConnection(), "[C]",
                                       OpenVPN3DBus_interf_configuration,
                                       log_level, timestamp);
            if (!logfile.empty())
            {
                config_subscr->OpenLogFile(logfile);
            }

            if (colour)
            {
                config_subscr->SetColourScheme(Logger::LogColour::WHITE, Logger::LogColour::GREEN);
            }
        }

        if( !be_subscription && !session_subscr && !config_subscr)
        {
            throw CommandException("openvpn3-service-logger",
                                   "No logging enabled. Aborting.");
        }

        ProcessSignalProducer procsig(dbus.GetConnection(), OpenVPN3DBus_interf_logger, "Logger");

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
        g_unix_signal_add(SIGINT, stop_handler, main_loop);
        g_unix_signal_add(SIGTERM, stop_handler, main_loop);
        procsig.ProcessChange(StatusMinor::PROC_STARTED);
        g_main_loop_run(main_loop);
        procsig.ProcessChange(StatusMinor::PROC_STOPPED);
        g_main_loop_unref(main_loop);
        ret = 0;
    }
    catch (CommandException& excp)
    {
        throw;
    }


    // Clean up and exit
    if (be_subscription)
    {
        delete be_subscription;
    }
    if (session_subscr)
    {
        delete session_subscr;
    }
    if (config_subscr)
    {
        delete config_subscr;
    }

    return ret;
}

int main(int argc, char **argv)
{
    // This program does not require root privileges,
    // so if used - drop those privileges
    drop_root();

    SingleCommand argparser(argv[0], "OpenVPN 3 Logger", logger);
    argparser.AddVersionOption();
    argparser.AddOption("timestamp", 0,
                        "Print timestamps on each log entry");
    argparser.AddOption("colour", 0,
                        "Use colours to categorize log events");
    argparser.AddOption("config-manager", 0,
                        "Subscribe to configuration manager log entries");
    argparser.AddOption("session-manager", 0,
                        "Subscribe to session manager log entries");
    argparser.AddOption("session-manager-client-proxy", 0,
                        "Also include backend client messages proxied by the session manager");
    argparser.AddOption("vpn-backend", 0,
                        "Subscribe to VPN client log entries");
    argparser.AddOption("log-level", 0, "LEVEL", true,
                        "Set the log verbosity level (default 3)");
    argparser.AddOption("log-file", 0, "FILE", true,
                        "Log events to file");

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
