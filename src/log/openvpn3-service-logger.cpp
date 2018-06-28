//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#include "dbus/core.hpp"
#include "dbus-log.hpp"
#include "common/utils.hpp"
#include "common/cmdargparser.hpp"

using namespace openvpn;


class Logger : public LogConsumer
{
public:
    enum class LogColour : std::uint_fast8_t {
        BLACK,   BRIGHT_BLACK,
        RED,     BRIGHT_RED,
        GREEN,   BRIGHT_GREEN,
        YELLOW,  BRIGHT_YELLOW,
        BLUE,    BRIGHT_BLUE,
        MAGENTA, BRIGHT_MAGENTA,
        CYAN,    BRIGHT_CYAN,
        WHITE,   BRIGHT_WHITE
    };


    Logger(GDBusConnection *dbuscon, std::string tag, std::string interf,
           unsigned int log_level = 3, bool timestamp = false)
        : LogConsumer(dbuscon, interf, ""),
          log_tag(tag),
          timestamp(timestamp),
          colourscheme(""),
          timestampcolour(""),
          colourreset("")
    {
        SetLogLevel(log_level);
    }

    void SetColourScheme(LogColour foreground, LogColour background)
    {
        const char * fgcode;
        const char * bgcode;

        switch (foreground)
        {
        case LogColour::BLACK:
                fgcode = "30m";
                break;
        case LogColour::RED:
                fgcode = "31m";
                break;
        case LogColour::GREEN:
                fgcode = "32m";
                break;
        case LogColour::YELLOW:
                fgcode = "33m";
                break;
        case LogColour::BLUE:
                fgcode = "34m";
                break;
        case LogColour::MAGENTA:
                fgcode = "35m";
                break;
        case LogColour::CYAN:
                fgcode = "36m";
                break;
        case LogColour::WHITE:
                fgcode = "37m";
                break;
        case LogColour::BRIGHT_BLACK:
                fgcode = "1;30m";
                break;
        case LogColour::BRIGHT_RED:
                fgcode = "1;31m";
                break;
        case LogColour::BRIGHT_GREEN:
                fgcode = "1;32m";
                break;
        case LogColour::BRIGHT_YELLOW:
                fgcode = "1;33m";
                break;
        case LogColour::BRIGHT_BLUE:
                fgcode = "1;34m";
                break;
        case LogColour::BRIGHT_MAGENTA:
                fgcode = "1;35m";
                break;
        case LogColour::BRIGHT_CYAN:
                fgcode = "1;36m";
                break;
        case LogColour::BRIGHT_WHITE:
                fgcode = "1;37m";
                break;
        default:
                fgcode = "0m";
                break;
        }

        switch (background)
        {
        case LogColour::BLACK:
        case LogColour::BRIGHT_BLACK:
                bgcode = "40m";
                break;
        case LogColour::RED:
        case LogColour::BRIGHT_RED:
                bgcode = "41m";
                break;
        case LogColour::GREEN:
        case LogColour::BRIGHT_GREEN:
                bgcode = "42m";
                break;
        case LogColour::YELLOW:
        case LogColour::BRIGHT_YELLOW:
                bgcode = "43m";
                break;
        case LogColour::BLUE:
        case LogColour::BRIGHT_BLUE:
                bgcode = "44m";
                break;
        case LogColour::MAGENTA:
        case LogColour::BRIGHT_MAGENTA:
                bgcode = "45m";
                break;
        case LogColour::CYAN:
        case LogColour::BRIGHT_CYAN:
                bgcode = "46m";
                break;
        case LogColour::WHITE:
        case LogColour::BRIGHT_WHITE:
                bgcode = "47m";
                break;
        default:
                bgcode = "0m";
                break;
        }

        colourreset = "\033[0m";
        timestampcolour = colourreset + "\033[37m\033[" + std::string(bgcode);
        colourscheme = colourreset + "\033[" + std::string(fgcode)
                        + "\033[" + std::string(bgcode);
    }


    /**
     *  Adds a LogGroup to be excluded when consuming log events
     * @param exclgrp LogGroup to exclude
     */
    void AddExcludeFilter(LogGroup exclgrp)
    {
        exclude_loggroup.push_back(exclgrp);
    }


    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogGroup group, const LogCategory catg,
                         const std::string msg)
    {
        for (const auto& e : exclude_loggroup)
        {
            if (e == group)
            {
                return;  // Don't do anything if this LogGroup is filtered
            }
        }

        std::cout << timestampcolour << (timestamp ? GetTimestamp() : "")
                  << colourscheme
                  << log_tag << ":: sender=" << sender
                  << ", interface=" << interface
                  << ", path=" << object_path
                  << std::endl
                  << (timestamp ? "                    " : "       ")
                  << LogPrefix(group, catg) << msg
                  << std::endl << colourreset;

    }

private:
    std::string log_tag;
    bool timestamp;
    std::string colourscheme;
    std::string timestampcolour;
    std::string colourreset;
    std::vector<LogGroup> exclude_loggroup;
};

static int logger(ParsedArgs args)
{
    std::cout << get_version(args.GetArgv0()) << std::endl;

    int ret = 0;
    bool timestamp = args.Present("timestamp");
    bool colour = args.Present("colour");

    unsigned int log_level = 3;
    if (args.Present("log-level"))
    {
        log_level = std::atoi(args.GetValue("log-level", 0).c_str());
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    Logger * be_subscription = nullptr;
    Logger * session_subscr = nullptr;
    Logger * config_subscr = nullptr;
    try
    {
        if (args.Present("vpn-backend"))
        {
            be_subscription = new Logger(dbus.GetConnection(), "[B]",
                                         OpenVPN3DBus_interf_backends,
                                         log_level, timestamp);
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
