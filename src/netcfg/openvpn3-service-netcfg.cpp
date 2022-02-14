//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg.cpp
 *
 * @brief  OpenVPN 3 D-Bus service managing network configurations
 */

#include <string>
#include <cap-ng.h>
#include <cassert>

#include <openvpn/common/base64.hpp>

#include "common/utils.hpp"
#include "common/lookup.hpp"
#include "common/cmdargparser.hpp"
#include "dbus/core.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "log/ansicolours.hpp"
#include "log/proxy-log.hpp"
#include "log/core-dbus-logbase.hpp"

#include "netcfg.hpp"
#include "netcfg-options.hpp"
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
    catch (const LookupException& excp)
    {
        throw CommandException("openvpn3-service-netcfg", excp.str());
    }

    capng_flags_t flags = (capng_flags_t) (CAPNG_DROP_SUPP_GRP | CAPNG_CLEAR_BOUNDING);
    int res = capng_change_id(uid, gid, flags);
    if (0 != res)
    {
        std::cout << "Result: " << res << std::endl;
        throw CommandException("openvpn-service-netcfg",
                               "** FATAL** Failed to drop to user/group to "
                               OPENVPN_USERNAME "/" OPENVPN_GROUP);
    }
}


static void apply_capabilities(ParsedArgs::Ptr args,
                               const NetCfgOptions& opts)
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
        capng_update(CAPNG_ADD, (capng_type_t) (CAPNG_EFFECTIVE|CAPNG_PERMITTED),
                      CAP_NET_ADMIN);

        // CAP_DAC_OVERRIDE is needed to be allowed to overwrite /etc/resolv.conf
        if (args->Present("resolv-conf"))
        {
            capng_update(CAPNG_ADD, (capng_type_t) (CAPNG_EFFECTIVE|CAPNG_PERMITTED),
                         CAP_DAC_OVERRIDE);
        }

        if (RedirectMethod::BINDTODEV ==  opts.redirect_method)
        {
            // We need this to be able to call setsockopt with SO_BINDTODEVICE
            capng_update(CAPNG_ADD, (capng_type_t) (CAPNG_EFFECTIVE|CAPNG_PERMITTED),
                         CAP_NET_RAW);

        }
    }
#ifdef OPENVPN_DEBUG
    if (!args->Present("run-as-root"))
#endif
    {
        // With the capapbility set, no root account access is needed
        drop_root_ng();
    }
    capng_apply(CAPNG_SELECT_CAPS);
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
    std::ostream  *logfile = nullptr;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    openvpn::base64_init_static();

    if (args->Present("log-file"))
    {
        std::string fname = args->GetLastValue("log-file");

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

    //
    // Prepare dropping capabilities and user privileges
    //
    apply_capabilities(args, netcfgopts);

    int log_level = 4;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetLastValue("log-level").c_str());
    }

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
        DBus dbus(G_BUS_TYPE_SYSTEM);
        dbus.Connect();

        // If we do unicast (!broadcast), attach to the log service
        if (!netcfgopts.signal_broadcast)
        {
            logservice = LogServiceProxy::AttachInterface(dbus.GetConnection(),
                                                          OpenVPN3DBus_interf_netcfg);
            logservice->Attach(OpenVPN3DBus_interf_netcfg + ".core");
        }

        // Initialize logging in the OpenVPN 3 Core library
        openvpn::CoreDBusLogBase corelog(dbus.GetConnection(),
                                         OpenVPN3DBus_interf_netcfg + ".core",
                                         LogGroup::NETCFG,
                                         logwr.get());
        corelog.SetLogLevel(log_level);

        std::cout << get_version(args->GetArgv0()) << std::endl;

        //
        // DNS resolver integrations
        //
        DNS::ResolverBackendInterface::Ptr resolver_be = nullptr;

        DNS::ResolvConfFile::Ptr resolvconf = nullptr;
        if (args->Present("resolv-conf"))
        {
            std::string rsc =  args->GetLastValue("resolv-conf");

            // We need to preserve a ResolvConfFile pointer to be able
            // to access the DNS::ResolvConfFile::Restore() method
            // when shutting down.
            resolvconf = new DNS::ResolvConfFile(rsc, rsc + ".ovpn3bak");
            resolver_be = resolvconf;
        }

        if (args->Present("systemd-resolved"))
        {
            try
            {
                resolver_be = new DNS::SystemdResolved(dbus.GetConnection());
            }
            catch (const DNS::resolved::Exception& excp)
            {
                std::cerr << "*** ERROR *** "<< excp.what() << std::endl;
            }
            catch (const NetCfgException& excp)
            {
                std::cerr << "*** ERROR *** "<< excp.what() << std::endl;
            }
        }

        DNS::SettingsManager::Ptr resolvmgr = nullptr;
        if (resolver_be)
        {
            resolvmgr = new DNS::SettingsManager(resolver_be);
        }

        NetworkCfgService netcfgsrv(dbus.GetConnection(), resolvmgr,
                                    logwr.get(), netcfgopts);
        netcfgsrv.SetDefaultLogLevel(log_level);

        // Prepare GLib Main loop
        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        // Setup idle-exit logic
        IdleCheck::Ptr idle_exit;
        if (idle_wait_min > 0)
        {
            idle_exit.reset(new IdleCheck(main_loop,
                                          std::chrono::minutes(idle_wait_min)));
            netcfgsrv.EnableIdleCheck(idle_exit);
        }
        else
        {
            // If we don't use the IdleChecker, handle these signals
            // in via the stop_handler instead
            g_unix_signal_add(SIGINT, stop_handler, main_loop);
            g_unix_signal_add(SIGTERM, stop_handler, main_loop);
        }
        netcfgsrv.Setup();

        if (idle_wait_min > 0)
        {
            idle_exit->Enable();
        }

        // Start the main loop
        g_main_loop_run(main_loop);
        usleep(500);
        g_main_loop_unref(main_loop);

        if (logservice)
        {
            logservice->Detach(OpenVPN3DBus_interf_netcfg);
            logservice->Detach(OpenVPN3DBus_interf_netcfg + ".core");
        }

        if (idle_wait_min > 0)
        {
            idle_exit->Disable();
            idle_exit->Join();
        }

        // Explicitly restore the resolv.conf file, if configured
        try
        {
            if (resolvconf)
            {
                resolvconf->Restore();
            }
        }
        catch (std::exception& e2)
        {
            std::cout << "** ERROR ** Failed restoring resolv.conf: "
                      << e2.what() << std::endl;
            if (0 == exit_code)
            {
                exit_code = 4;
            }
        }
    }
    catch (const LogServiceProxyException& excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        exit_code = 3;
    }
    catch (std::exception& excp)
    {
        std::cout << "FATAL ERROR: " << excp.what() << std::endl;
        exit_code = 3;
    }

    openvpn::base64_uninit_static();

    return exit_code;
}

int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Network Configuration Manager",
                            netcfg_main);
    argparser.AddVersionOption();
    argparser.AddOption("log-level", "LOG-LEVEL", true,
                        "Sets the default log verbosity level (valid values 0-6, default 4)");
    argparser.AddOption("log-file", "FILE" , true,
                        "Write log data to FILE.  Use 'stdout:' for console logging.");
    argparser.AddOption("colour", 0,
                        "Make the log lines colourful");
    argparser.AddOption("signal-broadcast", 0,
                        "Broadcast all D-Bus signals instead of targeted unicast");
    argparser.AddOption("idle-exit", "MINUTES", true,
                        "How long to wait before exiting if being idle. "
                        "0 disables it (Default: 5 minutes)");
    argparser.AddOption("resolv-conf", "FILE", true,
                        "Use file based resolv.conf management, based using FILE");
    argparser.AddOption("systemd-resolved", 0,
                        "Use systemd-resolved for configuring DNS resolver settings");
    argparser.AddOption("redirect-method", "METHOD", true,
                        "Method to use if --redirect-gateway is in use for VPN server redirect. "
                        "Methods: host-route (default), bind-device, none");
    argparser.AddOption("set-somark", "MARK", true,
                        "Set the specified so mark on all VPN sockets.");
    argparser.AddOption("state-dir", 0, "DIRECTORY", true,
                        "Directory where to save the runtime configuration settings");
#if OPENVPN_DEBUG
    argparser.AddOption("disable-capabilities", 0,
                        "Do not restrcit any process capabilties (INSECURE)");
    argparser.AddOption("run-as-root", 0,
                        "Keep running as root and do not drop privileges (INSECURE)");
#endif

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandArgBaseException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
    catch (const std::exception& excp)
    {
        std::cout << "*** ERROR ***   " << excp.what() << std::endl;
        return 3;
    }
}
