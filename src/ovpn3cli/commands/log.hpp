//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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

/**
 * @file   log.hpp
 *
 * @brief  Commands related to receive log entries from various sessions
 */

#include "dbus/core.hpp"
#include "common/timestamp.hpp"
#include "log/proxy-log.hpp"

using namespace openvpn;


/**
 *  Simple class to handle Log signal events
 */
class LoggerBase : public LogConsumer
{
public:
    /**
     *  When instantiating a new Logger object, it will subscribe to
     *  Log signals appearing on a specific interface and D-Bus object path.
     *  If these are empty, it will listen to any Log signals
     *
     * @param dbscon       D-Bus connection to use for subscribing
     * @param interf       std::string containing the interface to subscribe to
     * @param object_path  std::string with the D-Bus object path to subscribe to
     */
    LoggerBase(GDBusConnection * dbscon, std::string interf,
           std::string object_path)
        : LogConsumer(dbscon, interf, object_path)
    {
    }


    /**
     *  This method is called on each Log signal event.
     *
     * @param sender       std::string with the sender of the Log event sender
     * @param interface    std::string with the interface the Log signal origins from
     * @param object_path  std::string with the D-Bus object path of the Log event
     * @param group        LogGroup indicator of the Log event
     * @param catg         LogCategory indicator of the Log event
     * @param msg          std::string carrying the Log message
     */
    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogEvent& logev)
    {
        std::cout << GetTimestamp() << logev << std::endl;
    }
};


class Logger : public LoggerBase,
               public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<Logger> Ptr;

    /**
     *  When instantiating a new Logger object, it will subscribe to
     *  Log signals appearing on a specific interface and D-Bus object path.
     *  If these are empty, it will listen to any Log signals
     *
     * @param dbscon       D-Bus connection to use for subscribing
     * @param interf       std::string containing the interface to subscribe to
     * @param object_path  std::string with the D-Bus object path to subscribe to
     */
    Logger(GDBusConnection * dbscon, std::string interf,
           std::string object_path)
        : LoggerBase(dbscon, interf, object_path)
    {
    }

};


class SessionLogger : public LoggerBase,
                      public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<SessionLogger> Ptr;

    SessionLogger(GDBusConnection * dbscon, std::string interf,
                  std::string object_path, GMainLoop *main_loop)
        : LoggerBase(dbscon, interf, object_path),
          main_loop(main_loop)
    {
        Subscribe("StatusChange");
    }

    void ProcessSignal(const std::string sender_name,
                       const std::string object_path,
                       const std::string interface_name,
                       const std::string signal_name,
                       GVariant *parameters) override
    {
        if ("StatusChange" == signal_name)
        {
            StatusEvent stev(parameters);

            std::cout << GetTimestamp() << ">> " << stev << std::endl;

            // If the session was removed, stop the logger
            if (stev.Check(StatusMajor::SESSION,
                           StatusMinor::SESS_REMOVED))
            {
                g_main_loop_quit((GMainLoop *) main_loop);
            }
        }
    }


private:
    GMainLoop *main_loop;
};

/**
 *  Simple log command which can retrieve Log events happening on a specific
 *  session path or coming from the configuration manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log_listen(ParsedArgs args)
{
    if (!args.Present("session-path") && !args.Present("config-events"))
    {
        throw CommandException("log",
                               "Either --session-path or --config-events must be provided");
    }

    // Prepare the main loop which will listen for Log events and process them
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    SessionLogger::Ptr session_log;
    Logger::Ptr config_log;
    bool log_flag_reset = false;
    std::string session_path = "";
    DBus dbuscon(G_BUS_TYPE_SYSTEM);
    dbuscon.Connect();

    if (args.Present("session-path"))
    {
        session_path = args.GetValue("session-path", 0);

        // Check if the session manager will forward log events for this
        // session.  If not, we must enable it - and if we do this, we
        // track that we modified this setting.
        OpenVPN3SessionProxy sesprx(G_BUS_TYPE_SYSTEM, session_path);
        if (!sesprx.CheckObjectExists())
        {
            throw CommandException("log",
                                   "Session not found");
        }

        if (!sesprx.GetReceiveLogEvents())
        {
            sesprx.SetReceiveLogEvents(true);
            log_flag_reset = true;
        }
        // Setup a Logger object for the provided session path
        session_log.reset(new SessionLogger(dbuscon.GetConnection(),
                                            OpenVPN3DBus_interf_sessions,
                                            session_path, main_loop));

        if (args.Present("log-level"))
        {
            try
            {
                int loglev = std::stoi(args.GetValue("log-level", 0));
                session_log->SetLogLevel(loglev);
                sesprx.SetLogVerbosity(loglev);
            }
            catch (std::exception& e)
            {
                throw CommandException("log",
                                       "Invalid --log-level argument: "
                                       + std::string(e.what()));
            }
        }
    }

    if (args.Present("config-events"))
    {
        // Set up a generic logger for configuration management related
        // Log events.  We don't provide an object path, as all events
        // occur with the same object path.  Filtering would not make any
        // difference on the end result.
        config_log.reset(new Logger(dbuscon.GetConnection(),
                                    OpenVPN3DBus_interf_configuration, ""));
    }

    // Start the main loop.  This will exit on SIGINT or SIGTERM signals only
    g_main_loop_run(main_loop);

    // If we enabled the receive_log_events property in a session object,
    // disable it again when we exit.
    if (!session_path.empty() && log_flag_reset)
    {
        OpenVPN3SessionProxy sesprx(G_BUS_TYPE_SYSTEM, session_path);
        if (sesprx.CheckObjectExists())
        {
            sesprx.SetReceiveLogEvents(false);
        }
    }

    // Clean-up and shut down.
    g_main_loop_unref(main_loop);

    return 0;
}


/**
 *  This command is used to query and manage the net.openvpn.v3.log service
 *  This service is a global service responsible for all logging.  Changes
 *  here affects all logging being done by this service on all attached
 *  log subscriptions.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log_service(ParsedArgs args)
{
    try
    {
        DBus dbus(G_BUS_TYPE_SYSTEM);
        dbus.Connect();
        LogServiceProxy logsrvprx(dbus.GetConnection());

        std::string old_loglev("");
        unsigned int curlev = logsrvprx.GetLogLevel();
        unsigned int newlev = curlev;
        if (args.Present("log-level"))
        {
            newlev = std::atoi(args.GetValue("log-level", 0).c_str());
            if ( curlev != newlev )
            {
                std::stringstream t;
                t << "            (Was: " << curlev << ")";
                old_loglev = t.str();
                logsrvprx.SetLogLevel(newlev);
            }
        }

        std::string old_tstamp("");
        bool curtstamp = logsrvprx.GetTimestampFlag();
        bool newtstamp = curtstamp;
        if (args.Present("timestamp"))
        {
            newtstamp = args.GetBoolValue("timestamp", 0);
            if ( curtstamp != newtstamp)
            {
                std::stringstream t;
                if (newtstamp)
                {
                    // simple alignment trick
                    t << " ";
                }
                t << "     (Was: "
                  << (curtstamp ? "enabled" : "disabled") << ")";
                old_tstamp = t.str();
                logsrvprx.SetTimestampFlag(newtstamp);
            }
        }

        std::string old_dbusdetails("");
        bool curdbusdetails = logsrvprx.GetDBusDetailsLogging();
        bool newdbusdetails = curdbusdetails;
        if (args.Present("dbus-details"))
        {
            newdbusdetails = args.GetBoolValue("dbus-details", 0);
            if ( curdbusdetails != newdbusdetails)
            {
                std::stringstream t;
                if (newdbusdetails)
                {
                    // simple alignment trick
                    t << " ";
                }
                t << "     (Was: "
                  << (curdbusdetails ? "enabled" : "disabled") << ")";
                old_dbusdetails = t.str();
                logsrvprx.SetDBusDetailsLogging(newdbusdetails);
            }
        }

        std::cout << " Attached log subscriptions: "
                  << logsrvprx.GetNumAttached() << std::endl;
        std::cout << "             Log timestamps: "
                  << (newtstamp ? "enabled" : "disabled")
                  << old_tstamp << std::endl;
        std::cout << "          Log D-Bus details: "
                  << (newdbusdetails ? "enabled" : "disabled")
                  << old_dbusdetails << std::endl;
        std::cout << "          Current log level: "
                  << newlev << old_loglev << std::endl;
    }
    catch (DBusProxyAccessDeniedException& excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service", rawerr);
    }
    catch (DBusException& excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service",
                               rawerr.substr(rawerr.rfind(":")));
    }
    return 0;
}

/**
 *  Declare all the supported commands and their options and arguments.
 *
 *  This function should only be called once by the main openvpn3 program,
 *  which sends a reference to the Commands argument parser which is used
 *  for this registration process
 *
 * @param ovpn3  Commands object where to register all the commands, options
 *               and arguments.
 */
void RegisterCommands_log(Commands& ovpn3)
{
    //
    //  session-start command
    //
    auto cmd = ovpn3.AddCommand("log",
                                "Receive log events as they occur",
                                cmd_log_listen);
    cmd->AddOption("session-path", "SESSION-PATH", true,
                   "Receive log events for a specific session",
                   arghelper_session_paths);
    cmd->AddOption("log-level", "LOG-LEVEL", true,
                   "Set the log verbosity level of messages to be shown (default: 4)",
                   arghelper_log_levels);
    cmd->AddOption("config-events",
                   "Receive log events issued by the configuration manager");

    auto service = ovpn3.AddCommand("log-service",
                               "Manage the OpenVPN 3 Log service",
                               cmd_log_service);
    service->AddOption("log-level", "LOG-LEVEL", true,
                       "Set the log level used by the log service.",
                       arghelper_log_levels);
    service->AddOption("timestamp", "true/false", true,
                       "Set the timestamp flag used by the log service",
                       arghelper_boolean);
    service->AddOption("dbus-details", "true/false", true,
                       "Log D-Bus sender, object path and method details of log sender",
                       arghelper_boolean);

}
