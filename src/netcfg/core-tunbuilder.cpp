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

#define OPENVPN_USE_SITNL

#include "core-tunbuilder.hpp"
#include <openvpn/tun/linux/client/tuncli.hpp>
#include "netcfg-device.hpp"


namespace openvpn
{

    class CoreTunbuilderImpl : public CoreTunbuilder
    {
        TUN_LINUX::Setup::Ptr tun;

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
                          TunNetlink::Setup::Config &config,
                          Stop *stop,
                          std::ostream& os)
        {
            if (!tun)
            {
                tun.reset(new TUN_LINUX::Setup);
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

            tbc->validate();

            // We ignore tbc.dns_servers and other DNS related items since
            // that is handled by a differrent service
            return tbc;
        }

    public:
        int establish(const NetCfgDevice& netCfgDevice) override
        {
            TunNetlink::Setup::Config config;
            config.layer = Layer::from_value(netCfgDevice.device_type);
            config.txqueuelen = netCfgDevice.txqueuelen;

            TunBuilderCapture::Ptr tbc = createTunbuilderCapture(netCfgDevice);

            //
            // We currently do not set config.iface_name to open the first
            // available tun device.
            //
            // config.ifname is a return value rather than an argument
            //
            return establish_tun(*tbc, config, nullptr, std::cout);
        }

        void teardown(bool disconnect) override
        {
            if(tun)
            {
                // the os parameter is not used
                tun->destroy(std::cerr);
            }
        }
        };

    CoreTunbuilder* getCoreBuilderInstance()
    {
        return new CoreTunbuilderImpl();
    }
}
