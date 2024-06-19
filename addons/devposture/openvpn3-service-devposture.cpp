//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <gdbuspp/connection.hpp>
#include <gdbuspp/service.hpp>
#include <iostream>

#include <common/cmdargparser.hpp>
#include <dbus/constants.hpp>
#include <log/ansicolours.hpp>
#include <log/logwriter.hpp>
#include <log/logwriters/implementations.hpp>
#include <log/dbus-log.hpp>
#include "service.hpp"


int devposture_main(ParsedArgs::Ptr args)
{
    std::cout << get_version(args->GetArgv0()) << std::endl;

    if (!args->Present("profile-dir"))
    {
        throw CommandException("openvpn3-service-devposture",
                               "--profile-dir is required");
    }

    // Enable automatic shutdown if the config manager is
    // idling for 1 minute or more.  By idling, it means
    // no configuration files is stored in memory.
    unsigned int idle_wait_min = 3;
    if (args->Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args->GetValue("idle-exit", 0).c_str());
    }

    // Open a log destination, if requested
    std::ofstream logfs{};
    std::ostream *logfile = nullptr;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    if (args->Present("log-file"))
    {
        std::string fname = args->GetValue("log-file", 0);

        if ("stdout:" != fname)
        {
            logfs.open(fname.c_str(), std::ios_base::app);
            logfile = &logfs;
        }
        else
        {
            logfile = &std::cout;
        }

        if (args->Present("colour"))
        {
            colourengine.reset(new ANSIColours());
            logwr.reset(new ColourStreamWriter(*logfile,
                                               colourengine.get()));
        }
        else
        {
            logwr.reset(new StreamLogWriter(*logfile));
        }
    }

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }


    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto devposture_service = DBus::Service::Create<DevPosture::Service>(dbuscon, logwr, log_level);
    devposture_service->LoadProtocolProfiles(args->GetValue("profile-dir", 0));

    devposture_service->PrepareIdleDetector(std::chrono::minutes(idle_wait_min));
    devposture_service->Run();

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Device Posture", devposture_main);
    argparser.AddVersionOption();
    argparser.AddOption("log-level",
                        "LOG-LEVEL",
                        true,
                        "Log verbosity level (valid values 0-6, default 3)");
    argparser.AddOption("log-file",
                        "FILE",
                        true,
                        "Write log data to FILE.  Use 'stdout:' for console logging.");
    argparser.AddOption("colour",
                        0,
                        "Make the log lines colourful");
    argparser.AddOption("idle-exit",
                        "MINUTES",
                        true,
                        "How long to wait before exiting if being idle. "
                        "0 disables it (Default: 3 minutes)");
    argparser.AddOption("profile-dir",
                        "DIRECTORY",
                        true,
                        "Directory containing device posture protocol profiles");

    try
    {
        // This program does not require root privileges,
        // so if used - drop those privileges
        drop_root();

        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException &excp)
    {
        std::cerr << "** ERROR ** " << excp.what();
        std::cerr << "\n            " << excp.debug_details() << "\n";
        return 2;
    }
    catch (const CommandException &excp)
    {
        std::cerr << excp.getCommand()
                  << ": ** ERROR ** " << excp.what() << "\n";
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
