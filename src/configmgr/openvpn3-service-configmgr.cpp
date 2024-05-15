//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2019-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include "build-config.h"
#include <gdbuspp/connection.hpp>
#include <gdbuspp/service.hpp>
#include <log/ansicolours.hpp>
#include <log/logwriter.hpp>
#include <log/logwriters/implementations.hpp>
#include <log/dbus-log.hpp>
#include <log/proxy-log.hpp>
#include <common/cmdargparser.hpp>
#include <common/utils.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include "configmgr-service.hpp"


using namespace openvpn;


static int config_manager(ParsedArgs::Ptr args)
{
    std::cout << get_version(args->GetArgv0()) << std::endl;

    // Enable automatic shutdown if the config manager is
    // idling for 1 minute or more.  By idling, it means
    // no configuration files is stored in memory.
    unsigned int idle_wait_min = 3;
    if (args->Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args->GetValue("idle-exit", 0).c_str());
    }

    // Open a log destination, if requested
    std::ofstream logfs;
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

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto configmgr_srv = DBus::Service::Create<ConfigManager::Service>(dbuscon, logwr);
    configmgr_srv->PrepareIdleDetector(std::chrono::minutes(idle_wait_min));

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

    if (args->Present("state-dir"))
    {
        configmgr_srv->SetStateDirectory(args->GetValue("state-dir", 0));
        umask(077);
    }

    configmgr_srv->SetLogLevel(log_level);

    configmgr_srv->Run();

    return 0;
}

int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Configuration Manager", config_manager);
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
    argparser.AddOption("state-dir",
                        0,
                        "DIRECTORY",
                        true,
                        "Directory where to save persistent data");

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
    catch (const CommandArgBaseException &excp)
    {
        std::cerr << excp.what() << "\n";
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
