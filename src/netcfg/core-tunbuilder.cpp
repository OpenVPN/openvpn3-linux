//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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

// Enable output of sitnl messages
#define DEBUG_SITNL 1

// Enable D-Bus logging of Core library
//
// The OPENVPN_EXTERN macro must be defined as well as
// the CoreDBusLogBase must be declared before other
// Core Library includes to enable the D-Bus logging
#define OPENVPN_EXTERN extern
#include "log/core-dbus-logbase.hpp"
#include <openvpn/common/platform.hpp>

// FIXME: This need to be included first because
//        it breaks if included after tuncli.hpp
#include "common/utils.hpp"

#include "core-tunbuilder.hpp"
#include <openvpn/tun/linux/client/tuncli.hpp>
#include "netcfg-device.hpp"
#include "netcfg-signals.hpp"

#define TUN_CLASS_SETUP TunLinuxSetup::Setup<TUN_LINUX>

namespace openvpn
{

    class CoreTunbuilderImpl : public CoreTunbuilder
    {
        TunLinuxSetup::Setup<TUN_LINUX>::Ptr tun;
        ActionList::Ptr remove_cmds;

        /**
         * Uses Tunbuilder to open a new tun device
         *
         * @param tbc     TunBuilderCapture that specifies the configuration
         * @param config  Tun Linux specific configuration
         * @param stop    (can be nullptr) allows to signal to stop the
         *                process
         * @param os      (currently unused) output to log output
         *
         * @return An fd that is the newly opened tun device
         */
        int establish_tun(const TunBuilderCapture &tbc,
                          TUN_CLASS_SETUP::Config &config,
                          Stop *stop,
                          std::ostream& os)
        {
            if (!tun)
            {
                tun.reset(new TUN_CLASS_SETUP());
            }

            return tun->establish(tbc, &config, nullptr, os);
        }


        /**
         * This create a TunBuilderCapture (OpenVPN3 internal representation)
         * from our internal representation in NetCfgDevice.
         *
         * @param netCfgDevice  The object of which the state should be
         *                      represented
         *
         * @return TunBuilderCapture object that represent the state of
         *         netCfgDevice
         */
        TunBuilderCapture::Ptr createTunbuilderCapture(const NetCfgDevice& netCfgDevice)
        {
            TunBuilderCapture::Ptr tbc;
            tbc.reset(new TunBuilderCapture);
            tbc->tun_builder_new();
            tbc->tun_builder_set_layer(netCfgDevice.device_type);
            tbc->tun_builder_set_mtu(netCfgDevice.mtu);

            for (const auto& ipaddr: netCfgDevice.vpnips)
            {
                tbc->tun_builder_add_address(ipaddr.address, ipaddr.prefix,
                                             ipaddr.gateway, ipaddr.ipv6,
                                             false);
            }
            tbc->tun_builder_set_remote_address(netCfgDevice.remote.address,
                                                netCfgDevice.remote.ipv6);

            // Add routes
            for (const auto& net: netCfgDevice.networks)
            {
                if (net.exclude)
                {
                    // -1 is "default/optional" value
                    tbc->tun_builder_exclude_route(net.address, net.prefix,
                                                   -1, net.ipv6);
                }
                else
                {
                    tbc->tun_builder_add_route(net.address, net.prefix,
                                               -1, net.ipv6);
                }
            }

            if (netCfgDevice.reroute_ipv4 || netCfgDevice.reroute_ipv6)
            {
                switch (netCfgDevice.options.redirect_method)
                {
                case RedirectMethod::HOST_ROUTE:
                case RedirectMethod::BINDTODEV:
                    // Add 'def1' style default routes
                    if (netCfgDevice.reroute_ipv4)
                    {
                        tbc->tun_builder_add_route("0.0.0.0", 1, -1, false);
                        tbc->tun_builder_add_route("128.0.0.0", 1, -1, false);
                    }
                    if (netCfgDevice.reroute_ipv6)
                    {
                        tbc->tun_builder_add_route("::", 1, -1, true);
                        tbc->tun_builder_add_route("8000::", 1, -1, true);
                    }
                    break;

                case RedirectMethod::NONE:
                    break;
                }
            }

            tbc->validate();

            // We ignore tbc.dns_servers and other DNS related items since
            // that is handled by a differrent service
            return tbc;
        }

    public:
        int establish(NetCfgDevice& netCfgDevice) override
        {
            TUN_CLASS_SETUP::Config config;
            config.layer = Layer::from_value(netCfgDevice.device_type);
            config.txqueuelen = netCfgDevice.txqueuelen;
            config.add_bypass_routes_on_establish = false;

#ifdef ENABLE_OVPNDCO
            if (netCfgDevice.dco_device)
            {
                config.dco = true;
                config.iface_name = netCfgDevice.device_name;
            }
#endif

            TunBuilderCapture::Ptr tbc = createTunbuilderCapture(netCfgDevice);

            //
            // For non-DCO, we currently do not set config.iface_name to
            // open the first available tun device.
            //
            // The config object below is a return value rather than
            // an argument
            //
            int ret = establish_tun(*tbc, config, nullptr, std::cout);

#ifdef ENABLE_OVPNDCO
            if (!netCfgDevice.dco_device)
            {
                netCfgDevice.set_device_name(config.iface_name);
            }
#else
            netCfgDevice.set_device_name(config.iface_name);
#endif

            doEstablishNotifies(netCfgDevice, config);

            return ret;
        }

        void doEstablishNotifies(const NetCfgDevice& netCfgDevice,
                                 const TUN_CLASS_SETUP::Config& config) const
        {
            // Announce the new interface
            NetCfgChangeEvent dev_ev(NetCfgChangeType::DEVICE_ADDED,
                                     config.iface_name, {});
            netCfgDevice.signal.NetworkChange(dev_ev);

            for (const auto& ipaddr : netCfgDevice.vpnips)
            {
                NetCfgChangeEvent chg_ev(NetCfgChangeType::IPADDR_ADDED,
                                         config.iface_name,
                                         {{"ip_address", ipaddr.address},
                                          {"prefix", std::to_string(ipaddr.prefix)},
                                          {"ip_version", (ipaddr.ipv6 ? "6" : "4")}});
                netCfgDevice.signal.NetworkChange(chg_ev);
            }

            // WARNING:  This is NOT optimal
            //           - but should work for most use cases
            //
            // We presume we have at most one IPv4 and one IPv6 address
            // on the virtual interface.  We will expose the gateway
            // addresses from these interfaces when announcing route
            // changes.
            //
            VPNAddress local4;
            VPNAddress local6;
            for (const auto& ip : netCfgDevice.vpnips)
            {
                if (ip.ipv6)
                {
                    local6 = ip;
                }
                else
                {
                    local4 = ip;
                }
            }

            // Announce routes related to this new interface
            for (const auto& net : netCfgDevice.networks)
            {
                NetCfgChangeType type;
                type = (net.exclude ? NetCfgChangeType::ROUTE_EXCLUDED
                                    : NetCfgChangeType::ROUTE_ADDED);
                NetCfgChangeEvent chg_ev(type, config.iface_name,
                                         {{"ip_version", (net.ipv6 ? "6" : "4")},
                                          {"subnet", net.address},
                                          {"prefix", std::to_string(net.prefix)},
                                          {"gateway", (net.ipv6 ? local6.gateway : local4.gateway)}});
                netCfgDevice.signal.NetworkChange(chg_ev);
            }
        }


        void teardown(const NetCfgDevice& ncdev, bool disconnect) override
        {
            if(tun)
            {
                // the os parameter is not used
                tun->destroy(std::cerr);
            }

            if (remove_cmds)
            {
                remove_cmds->execute_log();
            }

            // Announce the removed routes
            for (const auto& net: ncdev.networks)
            {
                if (net.exclude)
                {
                    continue;
                }
                NetCfgChangeEvent chg_ev(NetCfgChangeType::ROUTE_REMOVED,
                                         ncdev.get_device_name(),
                                         {{"ip_version", (net.ipv6 ? "6" : "4")},
                                          {"subnet", net.address},
                                          {"prefix", std::to_string(net.prefix)}});
                ncdev.signal.NetworkChange(chg_ev);
            }

            // Announce the removed interface
            for (const auto& ipaddr: ncdev.vpnips)
            {
                NetCfgChangeEvent chg_ev(NetCfgChangeType::IPADDR_REMOVED,
                                         ncdev.get_device_name(),
                                         {{"ip_address", ipaddr.address},
                                          {"prefix", std::to_string(ipaddr.prefix)},
                                          {"ip_version", (ipaddr.ipv6 ? "6" : "4")}});
                ncdev.signal.NetworkChange(chg_ev);
            }
            NetCfgChangeEvent chg_ev(NetCfgChangeType::DEVICE_REMOVED,
                                     ncdev.get_device_name(), {});
            ncdev.signal.NetworkChange(chg_ev);
        }
    };


    void protect_socket_somark(int fd, const std::string& remote, int somark)
    {
        OPENVPN_LOG("Protecting socket " + std::to_string(fd)
                    + " to '" + remote + " by setting SO_MARK to "
                    + std::to_string(somark));

        if (setsockopt(fd, SOL_SOCKET, SO_MARK, (int *) &somark, sizeof(int)))
        {
            throw NetCfgException(std::string("Setting SO_MARK failed: ")
                                  + strerror(errno));
        }
    }


    // Although this is not the cleanest approach the include of the action.hpp header
    // breaks if it is done from core-tunbuilder.hpp, so we keep the management of this list here
    std::map<pid_t, ActionList> protected_sockets;

    void cleanup_protected_sockets(pid_t pid)
    {
        // Execute remove commands for protected sockets
        auto psocket = protected_sockets.find(pid);
        if (psocket!= protected_sockets.end())
        {
            psocket->second.execute_log();
            protected_sockets.erase(psocket);
        }
    }

    void protect_socket_hostroute(const std::string& tun_intf, const std::string& remote, bool ipv6, pid_t pid)
    {
        std::vector<IP::Route> rtvec;
        ActionList add_cmds;


        // For now we assume that the will only have one socket it needs to protect and remove
        // the old protection when we get a new one. This can later be extended to supported multiple
        // sockets per pid if we need to
        protected_sockets.emplace(std::piecewise_construct, std::make_tuple(pid), std::make_tuple());

        OPENVPN_LOG("Protecting socket to '" + remote + " by adding host route");
        TUN_LINUX::TunMethods::add_bypass_route(tun_intf, remote, ipv6, &rtvec, add_cmds, protected_sockets[pid]);

        add_cmds.execute_log();
    }


    void protect_socket_binddev(int fd, const std::string & remote, bool ipv6)
    {
        std::string bestdev;

        if (ipv6)
        {
            IPv6::Addr bestgw;
            IP::Route6 hostroute(IPv6::Addr::from_string(remote), 128);
            if (TunNetlink::SITNL::net_route_best_gw(hostroute,
                                                     bestgw, bestdev) != 0)
            {
                throw NetCfgException("Failed retrieving IPv6 gateway for "
                                      + remote + " failed");
            }
        }
        else
        {
            IPv4::Addr bestgw;
            IP::Route4 hostroute(IPv4::Addr::from_string(remote), 32);
            if (TunNetlink::SITNL::net_route_best_gw(hostroute,
                                                     bestgw, bestdev) != 0)
            {
                throw NetCfgException("Failed retrieving IPv4 gateway for "
                                      + remote + " failed");
            }
        }

        OPENVPN_LOG("Protecting socket " + std::to_string(fd)
                    + " to '" + remote + "'(" + (ipv6 ? "inet6": "inet")
                    + ") by binding it to '" + bestdev + "'");

        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
                       (void *)bestdev.c_str(), strlen(bestdev.c_str())) < 0)
        {
            throw NetCfgException("Failed binding (SO_BINDTODEVICE) to "
                                  + bestdev + " failed: " + strerror(errno));
        }
    }


    CoreTunbuilder* getCoreBuilderInstance()
    {
        return new CoreTunbuilderImpl();
    }
}
