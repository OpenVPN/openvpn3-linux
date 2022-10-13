//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017-2022  David Sommerseth <davids@openvpn.net>
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
#include "logtag.hpp"
#include "logwriter.hpp"
#include "logwriters/implementations.hpp"
#include "ansicolours.hpp"
#include "service.hpp"
#include "service-configfile.hpp"

using namespace openvpn;

struct LogStringTag : public LogTag
{
    using Ptr = std::shared_ptr<LogStringTag>;

    LogStringTag(const std::string& t) : LogTag()
    {
        tag = std::string(t);
        hash = 0;
        encaps = false;
    }
    ~LogStringTag() = default;

    const std::string str(const bool override) const override
    {
        return tag;
    }
};


static int logger(ParsedArgs::Ptr args)
{
    int ret = 0;

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
        if (log_level > 6)
        {
            throw CommandException("openvpn3-service-logger",
                                   "--log-level can only be between 0 and 6");
        }
    }

    if (args->Present("system")
        && (args->Present("vpn-backend")
            || args->Present("session-manager")
            || args->Present("session-manager-client-proxy")
            || args->Present("config-manager")))
    {
        std::stringstream err;
        err << "--system cannot be combined with --config-manager, "
            << "--session-manager, --session-manager-client-proxy or"
            << "--vpn-backend";
        throw CommandException("openvpn3-service-logger", err.str());
    }

    // --idle-exit and --state-dir are only available with --service
    if ((args->Present("idle-exit") || args->Present("state-dir"))
        && !args->Present("service"))
    {
        throw CommandException("openvpn3-service-logger",
                               "--idle-exit or --state-dir cannot be used "
                               "without --service");
    }

    // Load and parse the log-service.json configuration file
    LogServiceConfigFile::Ptr cfgfile;
    if (args->Present("state-dir"))
    {
        cfgfile.reset(new LogServiceConfigFile(args->GetValue("state-dir", 0)));
        try
        {
            cfgfile->Load();
        }
        catch (const ConfigFileException&)
        {
            //  Ignore load errors; the file might be missing - which is fine.
        }
        try
        {
            cfgfile->CheckExclusiveOptions();
        }
        catch (const ConfigFileException&)
        {
            throw CommandException("openvpn3-service-logger",
                                   "Failed parsing configuration file: "
                                   + cfgfile->GetFilename());
        }
        catch (const ExclusiveOptionError& err)
        {
            std::stringstream e;
            e << "Error parsing configuration file (" << cfgfile->GetFilename()
              << "): " << err.what();
            throw CommandException("openvpn3-service-logger", e.str());
        }
        args->ImportConfigFile(cfgfile);
    }

    try
    {
        args->CheckExclusiveOptions({{"syslog", "journald", "log-file"},
                                     {"syslog", "journald", "colour"}});
    }
    catch (const ExclusiveOptionError& excp)
    {
        throw CommandException("openvpn3-service-logger",
                               excp.what());
    }


    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    GDBusConnection *dbusconn = dbus.GetConnection();
    Logger::Ptr be_subscription = nullptr;
    Logger::Ptr session_subscr = nullptr;
    Logger::Ptr config_subscr = nullptr;
    LogService::Ptr logsrv = nullptr;

    // Open a log destination
    std::ofstream logfs;
    std::streambuf * logstream;
    bool do_console_info = true;
    if (args->Present("log-file"))
    {
        logfs.open(args->GetValue("log-file", 0), std::ios_base::app);
        logstream = logfs.rdbuf();
    }
    else
    {
        logstream = std::cout.rdbuf();
        do_console_info = false;
    }
    std::ostream logfile(logstream);

    // Prepare the appropriate log writer
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;
#if HAVE_SYSTEMD
    if (args->Present("journald"))
    {
        do_console_info = true;
        logwr.reset(new JournaldWriter());
        logwr->EnableMessagePrepend(!args->Present("no-logtag-prefix"));
    }
    else
#endif // HAVE_SYSTEMD
    if (args->Present("syslog"))
     {
        do_console_info = true;
        int facility = LOG_DAEMON;
        if (args->Present("syslog-facility"))
        {
            try {
                std::string f = args->GetValue("syslog-facility", 0);
                facility = SyslogWriter::ConvertLogFacility(f);
            }
            catch (SyslogException& excp)
            {
                throw CommandException("openvpn3-service-logger",
                                       excp.what());
            }
        }
        logwr.reset(new SyslogWriter(args->GetArgv0(), facility));
     }
     else if (args->Present("colour"))
     {
         colourengine.reset(new ANSIColours());
         logwr.reset(new ColourStreamWriter(logfile,
                                            colourengine.get()));
     }
     else
     {
         logwr.reset(new StreamLogWriter(logfile));
     }
     logwr->EnableTimestamp(args->Present("timestamp"));
     logwr->EnableLogMeta(args->Present("service-log-dbus-details"));

     // Enable automatic shutdown if the logger is
     // idling for 10 minute or more.  By idling, it means
     // no services are attached to this log service
     IdleCheck::Ptr idle_exit;
     unsigned int idle_wait_min = 10;
     if (args->Present("idle-exit"))
     {
         idle_wait_min = std::atoi(args->GetValue("idle-exit", 0).c_str());
     }

     // Setup the log receivers
    try
    {
        logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::INFO,
                              get_version(args->GetArgv0())));
        logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::INFO,
                              "Log method: " + logwr->GetLogWriterInfo()));
        if (do_console_info)
        {
            std::cout << get_version(args->GetArgv0()) << std::endl;
            std::cout << "Log method: " << logwr->GetLogWriterInfo() << std::endl;
        }
        // Prepare the GLib GMainLoop
        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        if (args->Present("service"))
        {
            //  openvpn3-logger-service runs as a D-Bus log service
            //  where log senders request this service to subscribe to
            //  their log signals.
            //
            //  This service will be quite silent if
            //  openvpn3-service-{backendstart,client,configmgr,sessionmgr}
            //  uses --signal-broadcast, as they will not request any
            //  subscriptions from the log service.
            //

            logsrv.reset(new LogService(dbusconn, logwr.get(), log_level));
            if (cfgfile)
            {
                logsrv->SetConfigFile(cfgfile);
            }

            if (idle_wait_min > 0)
            {
                idle_exit.reset(new IdleCheck(main_loop,
                                              std::chrono::minutes(idle_wait_min)));
                logsrv->EnableIdleCheck(idle_exit);
                logwr->Write(LogEvent(LogGroup::LOGGER, LogCategory::INFO,
                        "Idle exit set to " + std::to_string(idle_wait_min)
                        + " minutes"));
            }
            else
            {
                // If we don't use the IdleChecker, handle these signals
                // in via the stop_handler instead
                g_unix_signal_add(SIGINT, stop_handler, main_loop);
                g_unix_signal_add(SIGTERM, stop_handler, main_loop);
#ifdef OPENVPN_DEBUG
                std::cout << "Idle exit is disabled" << std::endl;
#endif
            }
            logsrv->Setup();

            if (idle_wait_min > 0)
            {
                idle_exit->Enable();
            }
        }
        else
        {
            //  openvpn3-service-logger only captures broadcast Log signals
            //  and subscribes to specific broadcast senders.
            //
            //  If openvpn3-service-{backendstart,client,configmr,sessionmgr}
            //  uses --signal-broadcast, these subscribers will receive
            //  a lot more Log signals.  These services broadcasts very little
            //  information by default.

            unsigned int subscribers = 0;
            if (args->Present("vpn-backend"))
            {
                LogStringTag::Ptr vpnbetag;
                vpnbetag.reset(new LogStringTag("[B]"));

                be_subscription.reset(new Logger(dbusconn, logwr.get(),
                                                 vpnbetag, "",
                                                 OpenVPN3DBus_interf_backends,
                                                 log_level));
                ++subscribers;
            }

            if (args->Present("session-manager")
                || args->Present("session-manager-client-proxy"))
            {
                LogStringTag::Ptr smgrtag;
                smgrtag.reset(new LogStringTag("[S]"));

                session_subscr.reset(new Logger(dbusconn, logwr.get(),
                                                smgrtag, "",
                                                OpenVPN3DBus_interf_sessions,
                                                log_level));
                ++subscribers;
                if (!args->Present("session-manager-client-proxy"))
                {
                    // Don't forward log messages from the backend client which
                    // are proxied by the session manager.  Unless the
                    // --session-manager-client-proxy is used.  These log messages
                    // are also available via --vpn-backend, so this is more
                    // useful for debug reasons
                    session_subscr->AddExcludeFilter(LogGroup::CLIENT);
                }
            }

            if (args->Present("config-manager"))
            {
                LogStringTag::Ptr cmgrtag;
                cmgrtag.reset(new LogStringTag("[C]"));

                config_subscr.reset(new Logger(dbusconn, logwr.get(),
                                               cmgrtag, "",
                                               OpenVPN3DBus_interf_configuration,
                                               log_level));
                ++subscribers;
            }

            if ((subscribers > 1) && colourengine)
            {
                // If having subscripbtions for different services, it
                // is easier to separate them by grouping services (LogGroup)
                // by colour than by LogCategory
                colourengine->SetColourMode(ColourEngine::ColourMode::BY_GROUP);
            }
        }

        if( !be_subscription && !session_subscr && !config_subscr && !logsrv)
        {
            throw CommandException("openvpn3-service-logger",
                                   "No logging enabled. Aborting.");
        }

        ProcessSignalProducer procsig(dbusconn, OpenVPN3DBus_interf_log, "Logger");

        procsig.ProcessChange(StatusMinor::PROC_STARTED);
        g_main_loop_run(main_loop);
        procsig.ProcessChange(StatusMinor::PROC_STOPPED);
        g_main_loop_unref(main_loop);

        // If the idle check is running, wait for it to complete
        if (idle_wait_min > 0)
        {
            idle_exit->Disable();
            idle_exit->Join();
        }

        ret = 0;
    }
    catch (CommandException& excp)
    {
        throw;
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
#if HAVE_SYSTEMD
    argparser.AddOption("no-logtag-prefix", 0,
                        "Do not prefix message lines with log tags (journald only)");
    argparser.AddOption("journald", 0,
                        "Send all log events to systemd-journald");
#endif // HAVE_SYSTEMD
    argparser.AddOption("syslog", 0,
                        "Send all log events to syslog");
    argparser.AddOption("syslog-facility", 0, "FACILITY", true,
                        "Use a specific syslog facility (Default: LOG_DAEMON)");
    argparser.AddOption("log-file", 0, "FILE", true,
                        "Log events to file");
    argparser.AddOption("service", 0,
                        "Run as a background D-Bus service");
    argparser.AddOption("service-log-dbus-details", 0,
                        "(Only with --service) Include D-Bus sender, path and method references in logs");
    argparser.AddOption("idle-exit", 0, "MINUTES", true,
                        "(Only with --service) How long to wait before exiting"
                        "if being idle. 0 disables it (Default: 10 minutes)");
    argparser.AddOption("state-dir", 0, "DIRECTORY", true,
                        "(Only with --service) Directory where to save the "
                        "service log settings");

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
