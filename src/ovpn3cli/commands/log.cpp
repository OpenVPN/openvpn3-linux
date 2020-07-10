//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
#include "common/cmdargparser.hpp"
#include "common/timestamp.hpp"
#include "log/dbus-log.hpp"
#include "log/logevent.hpp"
#include "log/proxy-log.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "sessionmgr/sessionmgr-events.hpp"
#include "../arghelpers.hpp"

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
        std::stringstream msg;
        msg << logev;
        std::vector<std::string> lines;
        std::string line;
        while(getline(msg, line, '\n'))
        {
            lines.push_back(line);
        }

        bool first = true;
        std::cout << GetTimestamp() << lines[0] << std::endl;
        for (const auto& l : lines)
        {
            if (first)
            {
                first = false;
                continue;
            }
            std::cout << "     " << l << std::endl;
        }
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
 *  Helper class to attach to log events from sessions
 *
 *  This class implements logic to also wait until the session manager
 *  signals a newly created session when the log attach is tied to a
 *  configuration profile name.  Once the session is found, the SessionLogger
 *  class takes over the log event handling itself.  The LogAttach object
 *  will also stop the logging once the session manager signals the session
 *  has been destroyed.
 *
 */
class LogAttach : public DBusSignalSubscription
{
public:
    LogAttach(GMainLoop *main_loop,
              GDBusConnection *dbuscon)
        : DBusSignalSubscription(dbuscon,
                                 OpenVPN3DBus_name_sessions,
                                 OpenVPN3DBus_interf_sessions,
                                 OpenVPN3DBus_rootp_sessions),
        mainloop(main_loop)
    {
        manager.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM,
                                               OpenVPN3DBus_rootp_sessions));
        Subscribe("SessionManagerEvent");
    }


    void AttachByPath(const std::string path)
    {
        session_path = path;
        create_session_proxy(session_path);
    }


    void AttachByConfig(const std::string config)
    {
        config_name = config;
        lookup_config_name(config_name);
        create_session_proxy(session_path);
    }


    void AttachByInterface(const std::string interf)
    {
        tun_interf = interf;
        lookup_interface(tun_interf);
        create_session_proxy(session_path);
    }


    void SetLogLevel(const unsigned int loglvl)
    {
        log_level = loglvl;
    }


    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters) override
    {
        if ("SessionManagerEvent" == signal_name)
        {
            SessionManager::Event ev(parameters);

            switch (ev.type)
            {
            case SessionManager::EventType::SESS_CREATED:
                if (session_proxy)
                {
                    // If we already have a running session proxy,
                    // ignore new sessions
                    return;
                }
                if (!config_name.empty())
                {
                    lookup_config_name(config_name);
                }
                else if (!tun_interf.empty())
                {
                    lookup_interface(tun_interf);
                }

                create_session_proxy(session_path);
                break;

            case SessionManager::EventType::SESS_DESTROYED:
                if (0 != ev.path.compare(session_path))
                {
                    // This event is not related to us, ignore it
                    return;
                }

                cleanup();
                std::cout << "Session closed" << std::endl;
                g_main_loop_quit((GMainLoop *) mainloop);
                break;

            case SessionManager::EventType::UNSET:
            default:
                // Ignore unknown types silently
                break;
            }
        }
    }

private:
    GMainLoop *mainloop = nullptr;
    std::string session_path{""};
    std::string config_name{""};
    std::string tun_interf{""};
    std::unique_ptr<OpenVPN3SessionProxy> manager = nullptr;
    std::unique_ptr<OpenVPN3SessionProxy> session_proxy = nullptr;
    SessionLogger::Ptr session_log = nullptr;
    unsigned int log_level = 0;
    bool log_flag_reset = false;
    bool wait_notification = false;


    void lookup_config_name(const std::string cfgname)
    {
        // We need to try a few times, as the SESS_CREATED event comes
        // quite early and the session object itself might not be registered
        // quite yet.  This needs to be a tight loop to catch the earliest
        // log messages, as certificate only based authentication may do
        // the full connection establishing in a little bit less than 200ms.
        for (unsigned int i = 250; i > 0; i--)
        {
            std::vector<std::string> paths = manager->LookupConfigName(cfgname);
            if (1 < paths.size())
            {
                throw CommandException("log",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            else if (1 == paths.size())
            {
                // If only a single path is found, that's the one we're
                // looking for.
                session_path = paths.at(0);
                return;
            }
            else
            {
                // If no paths has been found, we wait until next call
                if (!wait_notification)
                {
                    std::cout << "Waiting for session to start ..."
                              << std::flush;
                    wait_notification = true;
                }
            }
            usleep(10000); // 10ms
        }
    }


    void lookup_interface(const std::string interf)
    {
        // This method does not have any retry logic as @lookup_config_name()
        // the tun interface name will appear quite late in the connection
        // logic, making it less useful for non-established VPN sessions.
        // The tun device name is first known after a successful connection.
        try
        {
            session_path = manager->LookupInterface(interf);
        }
        catch (const TunInterfaceException& excp)
        {
            throw CommandException("log", std::string(excp.GetRawError()));
        }
        catch (const std::exception& excp)
        {
            throw CommandException("log", std::string(excp.what()));
        }
    }


    /**
     *  Establish a connection to the session object for a specific
     *  VPN session, enable session logging and sets the log level.
     *
     *  The input path is used to ensure a SESS_CREATED SessionManager::Event
     *  is tied to the VPN session we expect.
     *
     * @param path  std::string of the VPN session to attach to.
     */
    void create_session_proxy(std::string path)
    {
        if (path.empty()
            || (0 != session_path.compare(path)))
        {
            return;
        }

        if (wait_notification)
        {
            std::cout << " Done" << std::endl;
        }

        // Check if the session manager will forward log events for this
        // session.  If not, we must enable it - and if we do this, we
        // track that we modified this setting.
        session_proxy.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, path));
        if (!session_proxy->CheckObjectExists())
        {
            throw CommandException("log",
                                   "Session not found");
        }

        std::cout << "Attaching to session " << path << std::endl;

        if (!session_proxy->GetReceiveLogEvents())
        {
            session_proxy->SetReceiveLogEvents(true);
            log_flag_reset = true;
        }

        if (log_level > 0)
        {
            try
            {
                session_proxy->SetLogVerbosity(log_level);
            }
            catch (std::exception& e)
            {
                // Log level might be incorrect, but we don't exit because
                // of that - as it might be we've been waiting for a session
                // to start.
                std::cerr << "** WARNING ** Log level not set: "
                          << e.what() << std::endl;
            }
        }
        // Setup a Logger object for the provided session path
        session_log.reset(new SessionLogger(GetConnection(),
                                            OpenVPN3DBus_interf_sessions,
                                            session_path, mainloop));
    }


    void cleanup()
    {
        // If we enabled the receive_log_events property in a session object,
        // disable it again when we exit.
        if (session_proxy && log_flag_reset)
        {
            if (session_proxy->CheckObjectExists())
            {
                session_proxy->SetReceiveLogEvents(false);
            }
        }
    }
};


/**
 *  openvpn3 log
 *
 *  Simple log command which can retrieve Log events happening on a specific
 *  session path or coming from the configuration manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log(ParsedArgs::Ptr args)
{
    if (!args->Present("session-path")
        && !args->Present("config")
        && !args->Present("interface")
        && !args->Present("config-events"))
    {
        throw CommandException("log",
                               "Either --session-path, --config, --interface "
                               "or --config-events must be provided");
    }

    // Prepare the main loop which will listen for Log events and process them
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    Logger::Ptr config_log;
    std::unique_ptr<LogAttach> logattach;
    DBus dbuscon(G_BUS_TYPE_SYSTEM);
    dbuscon.Connect();

    if (args->Present("session-path") || args->Present("config")
        || args->Present("interface"))
    {
        std::string session_path = "";
        logattach.reset(new LogAttach (main_loop, dbuscon.GetConnection()));

        if (args->Present("log-level"))
        {
            logattach->SetLogLevel(std::stoi(args->GetValue("log-level", 0)));
        }

        if (args->Present("config"))
        {
            logattach->AttachByConfig(args->GetValue("config", 0));
        }
        else if (args->Present("interface"))
        {
            logattach->AttachByInterface(args->GetValue("interface", 0));
        }
        else
        {
            logattach->AttachByPath(args->GetValue("session-path", 0));
        }
    }

    if (args->Present("config-events"))
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


    // Clean-up and shut down.
    g_main_loop_unref(main_loop);

    return 0;
}


std::string arghelper_log_config_names()
{
    return arghelper_config_names() + arghelper_config_names_sessions();
}


SingleCommand::Ptr prepare_command_log()
{
    //
    //  log command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("log",
                                "Receive log events as they occur",
                                cmd_log));
    cmd->AddOption("session-path", "SESSION-PATH", true,
                   "Receive log events for a specific session",
                   arghelper_session_paths);
    cmd->AddOption("config", 'c', "CONFIG-NAME", true,
                   "Alternative to --session-path, where configuration "
                   "profile name is used instead",
                   arghelper_log_config_names);
    cmd->AddOption("interface", 'I', "INTERFACE", true,
                   "Alternative to --session-path, where tun interface name "
                   "is used instead",
                   arghelper_managed_interfaces);
    cmd->AddOption("log-level", "LOG-LEVEL", true,
                   "Set the log verbosity level of messages to be shown (default: 4)",
                   arghelper_log_levels);
    cmd->AddOption("config-events",
                   "Receive log events issued by the configuration manager");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
