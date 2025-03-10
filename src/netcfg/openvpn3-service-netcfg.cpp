//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2019-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2021-  Heiko Hund <heiko@openvpn.net>
//

/**
 * @file   netcfg.cpp
 *
 * @brief  OpenVPN 3 D-Bus service managing network configurations
 */

#include <string>
#include <cap-ng.h>

#include <openvpn/common/base64.hpp>

#include "build-config.h"
#include "common/utils.hpp"
#include "common/lookup.hpp"
#include "common/cmdargparser.hpp"

#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"
#include "log/ansicolours.hpp"
#include "log/proxy-log.hpp"
#include "log/core-dbus-logger.hpp"

#include "netcfg-options.hpp"
#include "netcfg-service.hpp"
#include "netcfg-service-handler.hpp"
#include "netcfg/dns/settings-manager.hpp"
#include "netcfg/dns/resolvconf-file.hpp"
#include "netcfg/dns/systemd-resolved.hpp"

using namespace NetCfg;


static void drop_root_ng()
{
    uid_t uid;
    gid_t gid;
    try
    {
        uid = lookup_uid(OPENVPN_USERNAME);
        gid = lookup_gid(OPENVPN_GROUP);
    }
    catch (const LookupException &excp)
    {
        throw CommandException("openvpn3-service-netcfg", excp.str());
    }

    capng_flags_t flags = (capng_flags_t)(CAPNG_DROP_SUPP_GRP | CAPNG_CLEAR_BOUNDING);
    int res = capng_change_id(uid, gid, flags);
    if (0 != res)
    {
        std::cout << "Result: " << res << std::endl;
        throw CommandException("openvpn-service-netcfg",
                               "** FATAL** Failed to drop to user/group to " OPENVPN_USERNAME "/" OPENVPN_GROUP);
    }
}


static void apply_capabilities(ParsedArgs::Ptr args,
                               const NetCfgOptions &opts)
{
    //
    // Prepare dropping capabilities and user privileges
    //
    capng_clear(CAPNG_SELECT_BOTH);
#ifdef OPENVPN_DEBUG
    if (!args->Present("disable-capabilities"))
#endif
    {
        // Need this capability to configure network and routing.
        int r = capng_update(CAPNG_ADD,
                             (capng_type_t)(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
                             CAP_NET_ADMIN);
        if (0 != r)
        {
            std::cerr << "** ERROR **  Failed to preserve CAP_NET_ADMIN "
                      << "capabilities" << std::endl;
            exit(3);
        }

        // CAP_DAC_OVERRIDE is needed to be allowed to overwrite /etc/resolv.conf
        if (args->Present("resolv-conf"))
        {
            r = capng_update(CAPNG_ADD,
                             (capng_type_t)(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
                             CAP_DAC_OVERRIDE);
            if (0 != r)
            {
                std::cerr << "** ERROR **  Failed to preserve CAP_DAC_OVERRIDE "
                          << "capabilities (needed due to --resolv-conf)"
                          << std::endl;
                exit(3);
            }
        }

        if (RedirectMethod::BINDTODEV == opts.redirect_method)
        {
            // We need this to be able to call setsockopt with SO_BINDTODEVICE
            r = capng_update(CAPNG_ADD,
                             (capng_type_t)(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
                             CAP_NET_RAW);
            if (0 != r)
            {
                std::cerr << "** ERROR **  Failed to preserve CAP_NET_RAW "
                          << "capabilities (needed due to --redirect-method "
                          << "bind-device)" << std::endl;
                exit(3);
            }
        }
    }
#ifdef OPENVPN_DEBUG
    if (!args->Present("run-as-root"))
#endif
    {
        // With the capapbility set, no root account access is needed
        drop_root_ng();
    }

    if (0 != capng_apply(CAPNG_SELECT_CAPS))
    {
        std::cerr << "** ERROR **  Failed to apply new capabilities. Aborting."
                  << std::endl;
        exit(3);
    }
}

int netcfg_main(ParsedArgs::Ptr args)
{
    if (0 != getegid() || 0 != geteuid())
    {
        throw CommandException("openvpn3-service-netcfg",
                               "This program must be started as root");
    }

    // Parse options which will be passed to the
    // NetCfg manager or device objects
    NetCfgConfigFile::Ptr netcfg_config;
    netcfg_config.reset(new NetCfgConfigFile{});
    NetCfgOptions netcfgopts(args, netcfg_config);

    //
    // Open a log destination, if requested
    //
    // This is opened before dropping privileges, to more easily tackle
    // scenarios where logging goes to a file in /var/log or other
    // directories where only root has access
    //
    std::ofstream logfs;
    std::ostream *logfile = nullptr;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    openvpn::base64_init_static();

    if (!netcfgopts.log_file.empty())
    {
        if ("stdout:" != netcfgopts.log_file)
        {
            logfs.open(netcfgopts.log_file.c_str(), std::ios_base::app);
            logfile = &logfs;
        }
        else
        {
            logfile = &std::cout;
        }

        if (netcfgopts.log_colour)
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

    //
    // Prepare dropping capabilities and user privileges
    //
    apply_capabilities(args, netcfgopts);

    // Enable automatic shutdown if the config manager is
    // idling for 1 minute or more.  By idling, it means
    // no configuration files is stored in memory.
    unsigned int idle_wait_min = 5;
    if (args->Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args->GetLastValue("idle-exit").c_str());
    }

    LogServiceProxy::Ptr logservice;
    int exit_code = 0;
    try
    {
        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);

        // Register this service for logging via net.openvpn.v3.log
        logservice = LogServiceProxy::AttachInterface(dbuscon,
                                                      Constants::GenInterface("netcfg"));
        logservice->Attach(Constants::GenInterface("netcfg.core"));

        std::cout << get_version(args->GetArgv0()) << std::endl;

        //
        // DNS resolver integrations
        //
        DNS::ResolverBackendInterface::Ptr resolver_be = nullptr;

        DNS::ResolvConfFile::Ptr resolvconf = nullptr;
        if (args->Present("resolv-conf"))
        {
            std::string rsc = args->GetLastValue("resolv-conf");

            // We need to preserve a ResolvConfFile pointer to be able
            // to access the DNS::ResolvConfFile::Restore() method
            // when shutting down.
            resolvconf = ResolvConfFile::Create(rsc, rsc + ".ovpn3bak");
            resolver_be = resolvconf;
        }

        if (args->Present("systemd-resolved"))
        {
            try
            {
                resolver_be = DNS::SystemdResolved::Create(dbuscon);
            }
            catch (const DNS::resolved::Exception &excp)
            {
                std::cerr << "*** ERROR *** " << excp.what() << std::endl;
            }
            catch (const NetCfgException &excp)
            {
                std::cerr << "*** ERROR *** " << excp.what() << std::endl;
            }
        }

        DNS::SettingsManager::Ptr resolvmgr = nullptr;
        if (resolver_be)
        {
            resolvmgr = DNS::SettingsManager::Create(resolver_be);
        }

        auto netcfgsrv = DBus::Service::Create<NetCfgService>(dbuscon,
                                                              resolvmgr,
                                                              logwr.get(),
                                                              netcfgopts);
        netcfgsrv->PrepareIdleDetector(std::chrono::minutes(idle_wait_min));

        netcfgsrv->Run();

        if (logservice)
        {
            logservice->Detach(Constants::GenInterface("netcfg"));
            logservice->Detach(Constants::GenInterface("netcfg.core"));
        }

        // Explicitly restore the resolv.conf file, if configured
        try
        {
            if (resolvconf)
            {
                resolvconf->Restore();
            }
        }
        catch (std::exception &e2)
        {
            std::cout << "** ERROR ** Failed restoring resolv.conf: "
                      << e2.what() << std::endl;
            if (0 == exit_code)
            {
                exit_code = 4;
            }
        }
    }
    catch (const LogServiceProxyException &excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        exit_code = 3;
    }
    catch (std::exception &excp)
    {
        std::cout << "FATAL ERROR: " << excp.what() << std::endl;
        exit_code = 3;
    }

    openvpn::base64_uninit_static();

    return exit_code;
}



int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Network Configuration Manager", netcfg_main);
    argparser.AddVersionOption();
    argparser.AddOption("log-level",
                        "LOG-LEVEL",
                        true,
                        "Sets the default log verbosity level (valid values 0-6, default 4)");
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
                        "0 disables it (Default: 5 minutes)");
    argparser.AddOption("resolv-conf",
                        "FILE",
                        true,
                        "Use file based resolv.conf management, based using FILE");
    argparser.AddOption("systemd-resolved",
                        0,
                        "Use systemd-resolved for configuring DNS resolver settings");
    argparser.AddOption("redirect-method",
                        "METHOD",
                        true,
                        "Method to use if --redirect-gateway is in use for VPN server redirect. "
                        "Methods: host-route (default), bind-device, none");
    argparser.AddOption("set-somark",
                        "MARK",
                        true,
                        "Set the specified so mark on all VPN sockets.");
    argparser.AddOption("state-dir",
                        0,
                        "DIRECTORY",
                        true,
                        "Directory where to save the runtime configuration settings");
#ifdef OPENVPN_DEBUG
    argparser.AddOption("disable-capabilities",
                        0,
                        "Do not restrcit any process capabilties (INSECURE)");
    argparser.AddOption("run-as-root",
                        0,
                        "Keep running as root and do not drop privileges (INSECURE)");
#endif

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandArgBaseException &excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
    catch (const std::exception &excp)
    {
        std::cout << "*** ERROR ***   " << excp.what() << std::endl;
        return 3;
    }
}
