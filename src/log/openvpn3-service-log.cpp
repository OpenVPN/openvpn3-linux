//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>
#include <gdbuspp/connection.hpp>

#include "build-config.h"
#include "build-version.h"
#include "dbus/constants.hpp"
#include "common/utils.hpp"
#include "common/cmdargparser.hpp"
#include "logwriter.hpp"
#include "logwriters/implementations.hpp"
#include "ansicolours.hpp"
#include "log-service.hpp"
#include "service-configfile.hpp"


static int logger_service(ParsedArgs::Ptr args)
{
    int ret = 0;

    // Load and parse the log-service.json configuration file
    LogService::Configuration servicecfg;
    LogServiceConfigFile::Ptr cfgfile = nullptr;
    if (args->Present("state-dir"))
    {
        cfgfile.reset(new LogServiceConfigFile(args->GetValue("state-dir", 0)));
        servicecfg.config_file = cfgfile;
        try
        {
            cfgfile->Load();
        }
        catch (const ConfigFileException &)
        {
            //  Ignore load errors; the file might be missing - which is fine.
        }
        try
        {
            cfgfile->CheckExclusiveOptions();
        }
        catch (const ConfigFileException &)
        {
            throw CommandException("openvpn3-service-log",
                                   "Failed parsing configuration file: "
                                       + cfgfile->GetFilename());
        }
        catch (const ExclusiveOptionError &err)
        {
            std::stringstream e;
            e << "Error parsing configuration file (" << cfgfile->GetFilename()
              << "): " << err.what();
            throw CommandException("openvpn3-service-log", e.str());
        }
        args->ImportConfigFile(cfgfile);
    }

    try
    {
        args->CheckExclusiveOptions({{"syslog", "journald", "log-file"},
                                     {"syslog", "journald", "colour"}});
    }
    catch (const ExclusiveOptionError &excp)
    {
        throw CommandException("openvpn3-service-log",
                               excp.what());
    }

    servicecfg.logfilter = Log::EventFilter::Create(3);
    if (args->Present("log-level"))
    {
        uint32_t loglev = std::atoi(args->GetValue("log-level", 0).c_str());
        servicecfg.logfilter->SetLogLevel(loglev);
    }

    if (args->Present("syslog"))
    {
        servicecfg.log_method = "syslog";
        if (args->Present("syslog-facility"))
        {
            try
            {
                std::string f = args->GetValue("syslog-facility", 0);
                servicecfg.syslog_facility = SyslogWriter::ConvertLogFacility(f);
            }
            catch (SyslogException &excp)
            {
                throw CommandException("openvpn3-service-log",
                                       excp.what());
            }
        }
    }
#ifdef HAVE_SYSTEMD
    else if (args->Present("journald"))
    {
        servicecfg.log_method = "journald";
    }
#endif
    else if (args->Present("logfile"))
    {
        servicecfg.log_method = "logfile";
        servicecfg.log_file = args->GetValue("log-file", 0);
    }
    else
    {
        servicecfg.log_method = "stdout:";
    }

    servicecfg.log_timestamp = args->Present("timestamp");
    servicecfg.log_dbus_details = args->Present("service-log-dbus-details");
    servicecfg.log_colour = args->Present("colour");

    // Open a log destination
    std::ofstream logfs{};
    std::streambuf *logstream = nullptr;
    bool do_console_info = true;
    if ("logfile" == servicecfg.log_method)
    {
        logfs.open(servicecfg.log_file, std::ios_base::app);
        logstream = logfs.rdbuf();
        servicecfg.log_method = "logfile";
    }
    else if ("stdout:" == servicecfg.log_method)
    {
        logstream = std::cout.rdbuf();
        do_console_info = false;
    }
    std::ostream logfile(logstream);

    // Prepare the appropriate log writer
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;
#ifdef HAVE_SYSTEMD
    if ("journald" == servicecfg.log_method)
    {
        do_console_info = true;
        logwr.reset(new JournaldWriter(Constants::GenServiceName("log")));
        logwr->EnableMessagePrepend(!args->Present("no-logtag-prefix"));
    }
    else
#endif // HAVE_SYSTEMD
        if ("syslog" == servicecfg.log_method)
        {
            do_console_info = true;
            logwr.reset(new SyslogWriter(args->GetArgv0(),
                                         servicecfg.syslog_facility));
        }
        else if (servicecfg.log_colour)
        {
            colourengine.reset(new ANSIColours());
            logwr.reset(new ColourStreamWriter(logfile,
                                               colourengine.get()));
        }
        else
        {
            logwr.reset(new StreamLogWriter(logfile));
        }
    logwr->EnableTimestamp(servicecfg.log_timestamp);
    logwr->EnableLogMeta(servicecfg.log_dbus_details);

    servicecfg.servicelog = LogService::Logger::Create(logwr,
                                                       LogGroup::LOGGER,
                                                       servicecfg.logfilter);

    auto bustype = DBus::BusType::SYSTEM;
#ifdef OPENVPN_DEBUG
    if (args->Present("use-session-bus"))
    {
        bustype = DBus::BusType::SESSION;
    }
#endif
    auto dbuscon = DBus::Connection::Create(bustype);
    auto main_service = DBus::Service::Create<LogService::MainService>(dbuscon);
    auto service_handler = main_service->CreateServiceHandler<LogService::ServiceHandler>(
        dbuscon, main_service->GetObjectManager(), servicecfg);

    // Enable automatic shutdown if the logger is
    // idling for 10 minute or more.  By idling, it means
    // no services are attached to this log service
    unsigned int idle_wait_min = 10;
    if (args->Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args->GetValue("idle-exit", 0).c_str());
        main_service->PrepareIdleDetector(std::chrono::minutes(idle_wait_min));
    }

    try
    {
        logwr->Write(Events::Log(LogGroup::LOGGER,
                                 LogCategory::INFO,
                                 get_version(args->GetArgv0())));
        logwr->Write(Events::Log(LogGroup::LOGGER,
                                 LogCategory::INFO,
                                 "Log method: " + logwr->GetLogWriterInfo()));
        if (do_console_info)
        {
            std::cout << get_version(args->GetArgv0()) << std::endl;
            std::cout << "Log method: " << logwr->GetLogWriterInfo() << std::endl;
        }

        if (idle_wait_min > 0)
        {
            logwr->Write(Events::Log(LogGroup::LOGGER, LogCategory::INFO, "Idle exit set to " + std::to_string(idle_wait_min) + " minutes"));
        }
        else
        {
#ifdef OPENVPN_DEBUG
            logwr->Write(Events::Log(LogGroup::LOGGER, LogCategory::INFO, "Idle exit is disabled"));
#endif
        }
        main_service->Run();

        ret = 0;
    }
    catch (CommandException &excp)
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

    SingleCommand argparser(argv[0], "OpenVPN 3 Logger", logger_service);
    argparser.AddVersionOption();
    argparser.AddOption("timestamp",
                        0,
                        "Print timestamps on each log entry");
    argparser.AddOption("colour",
                        0,
                        "Use colours to categorize log events");
    argparser.AddOption("log-level",
                        0,
                        "LEVEL",
                        true,
                        "Set the log verbosity level (default 3)");
#ifdef HAVE_SYSTEMD
    argparser.AddOption("no-logtag-prefix",
                        0,
                        "Do not prefix message lines with log tags (journald only)");
    argparser.AddOption("journald",
                        0,
                        "Send all log events to systemd-journald");
#endif // HAVE_SYSTEMD
    argparser.AddOption("syslog",
                        0,
                        "Send all log events to syslog");
    argparser.AddOption("syslog-facility",
                        0,
                        "FACILITY",
                        true,
                        "Use a specific syslog facility (Default: LOG_DAEMON)");
    argparser.AddOption("log-file",
                        0,
                        "FILE",
                        true,
                        "Log events to file");
    argparser.AddOption("service-log-dbus-details",
                        0,
                        "Include D-Bus sender, path and method references in logs");
    argparser.AddOption("idle-exit",
                        0,
                        "MINUTES",
                        true,
                        "How long to wait before exiting "
                        "if being idle. 0 disables it (Default: 10 minutes)");
    argparser.AddOption("state-dir",
                        0,
                        "DIRECTORY",
                        true,
                        "Directory where to save the service log settings");
#ifdef OPENVPN_DEBUG
    argparser.AddOption("use-session-bus",
                        0,
                        "Use the D-Bus session bus instead of the system bus, "
                        "for local testing");
#endif

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException &excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
