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


using namespace openvpn;


class ArgumentException : public std::exception
{
public:
    ArgumentException(int exitcode, const char * argv0, const std::string& message) noexcept
        : argv0(argv0),
          exitcode(exitcode),
          message(message)
    {
        fullmsg = std::string(argv0) + ": " + message;
    }


    ArgumentException(int exitcode, const char * argv0, const std::string&& message) noexcept
        : argv0(argv0),
          exitcode(exitcode),
          message(std::move(message))
    {
        fullmsg = std::string(argv0) + ": " + message;
    }


    virtual ~ArgumentException() throw()
    {
    }


    virtual const char* what() const throw()
    {
        return fullmsg.c_str();
    }


    std::string err() const noexcept
    {
        return fullmsg;
    }

    int get_exit_code() const noexcept
    {
        return exitcode;
    }

protected:
    const char * argv0;
    std::string fullmsg;

private:
    int exitcode;
    std::string message;
};


class UsageException : public ArgumentException
{
public:
    UsageException(int exitcode, const char * argv0) noexcept
        : ArgumentException(exitcode, argv0, std::string(""))
    {
        std::stringstream usage;
        usage << "Usage: " << argv0 << " "
              << "[--timestamp] "
              << "[--colour] "
              << "[--config-manager] "
              << "[--session-manager] "
              << "[--vpn-backend] "
              << "[-h | --help]"
              << std::endl << std::endl;

        usage << std::setw(20) << "--timestamp"
              << "  Print timestamps on each log entry"
              << std::endl;

        usage << std::setw(20) << "--colour"
              << "  Use colours to categorize log events"
              << std::endl;

        usage << std::setw(20) << "--config-manager"
              << "  Subscribe to configuration manager log entries"
              << std::endl;

        usage << std::setw(20) << "--session-manager"
              << "  Subscribe to session manager log entries"
              << std::endl;

        usage << std::setw(20) << "--vpn-backend"
              << "  Subscribe to VPN client log entries"
              << std::endl;

        fullmsg = std::move(usage.str());
    }
};


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
           bool timestamp = false)
        : LogConsumer(dbuscon, interf, ""),
          log_tag(tag),
          timestamp(timestamp),
          colourscheme(""),
          timestampcolour(""),
          colourreset("")
    {
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


    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogGroup group, const LogCategory catg,
                         const std::string msg)
    {
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
};



int main(int argc, char **argv)
{
    std::cout << get_version(argv[0]) << std::endl;

    // This program does not require root privileges,
    // so if used - drop those privileges
    drop_root();

    int ret = 0;
    bool timestamp = false;
    bool colour = false;
    Logger * be_subscription = nullptr;
    Logger * session_subscr = nullptr;
    Logger * config_subscr = nullptr;

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    try
    {
        for (int i = 1; i < argc; i++)
        {
            auto arg = std::string(argv[i]);

            if ("--timestamp" == arg)
            {
                timestamp = true;
            }
            else if ("--colour" == arg)
            {
                colour = true;
            }

            else if ("--vpn-backend" == arg)
            {
                be_subscription = new Logger(dbus.GetConnection(), "[B]", OpenVPN3DBus_interf_backends, timestamp);
                if (colour)
                {
                    be_subscription->SetColourScheme(Logger::LogColour::BRIGHT_BLUE, Logger::LogColour::BLACK);
                }
            }
            else if ("--session-manager" == arg)
            {
                session_subscr = new Logger(dbus.GetConnection(), "[S]", OpenVPN3DBus_interf_sessions, timestamp);
                if (colour)
                {
                    session_subscr->SetColourScheme(Logger::LogColour::BRIGHT_WHITE, Logger::LogColour::BLUE);
                }
            }
            else if ("--config-manager" == arg)
            {
                config_subscr = new Logger(dbus.GetConnection(), "[C]", OpenVPN3DBus_interf_configuration, timestamp);
                if (colour)
                {
                    config_subscr->SetColourScheme(Logger::LogColour::WHITE, Logger::LogColour::GREEN);
                }
            }
            else if ("--help" == arg || ("-h" == arg))
            {
                throw UsageException(1, argv[0]);
            }
            else
            {
                throw ArgumentException(2, argv[0], "Unknown argument: " + arg);
            }
        }

        if( !be_subscription && !session_subscr && !config_subscr)
        {
            throw ArgumentException(3, argv[0], "No logging enabled. Aborting.");
        }

        ProcessSignalProducer procsig(dbus.GetConnection(), OpenVPN3DBus_interf_logger, "Logger");

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
        g_unix_signal_add(SIGINT, stop_handler, main_loop);
        g_unix_signal_add(SIGTERM, stop_handler, main_loop);
        procsig.ProcessChange(StatusMinor::PROC_STARTED);
        g_main_loop_run(main_loop);
        procsig.ProcessChange(StatusMinor::PROC_STOPPED);
        g_main_loop_unref(main_loop);
    }
    catch (ArgumentException& excp)
    {
        std::cout << excp.err() << std::endl;
        ret = excp.get_exit_code();
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
