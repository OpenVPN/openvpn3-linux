//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
 * @file   core-client-netcfg.cpp
 *
 * @brief Class that extends the client class and also implements the
 *          tunbuilder methods using NetCfg service
 *
 */

#pragma once

#include <openvpn/tun/builder/base.hpp>

#include "netcfg/proxy-netcfg.hpp"
#include "backend-signals.hpp"

using namespace openvpn;


template <class T>
class NetCfgTunBuilder : public T
{
public:
    typedef RCPtr<NetCfgTunBuilder> Ptr;

    NetCfgTunBuilder(GDBusConnection* dbuscon, BackendSignals *signal)
        : disabled_dns_config(false),
          netcfgmgr(dbuscon),
          signal(signal)
    {
    }


    bool tun_builder_new() override
    {
        /* Destroy an old that might be still around */
        tun_builder_teardown(true);
        try
        {
            // Cleanup the old things
            networks.clear();

            std::string devpath = netcfgmgr.CreateVirtualInterface("o3tun");
            device.reset(netcfgmgr.getVirtualInterface(devpath));
            return true;
        }
        catch (NetCfgProxyException& e)
        {
            signal->LogError(std::string("Error creating virtual network device: ")
                             + e.what());
            return false;
        }
    }


    bool tun_builder_set_remote_address(const std::string& address, bool ipv6) override
    {
        device->SetRemoteAddress(address, ipv6);
        return true;
    }


    bool tun_builder_add_address(const std::string& address,
                                 int prefix_length,
                                 const std::string& gateway, // optional
                                 bool ipv6,
                                 bool net30) override
    {
        /* We ignore net30 and gateway here for now */
        try
        {
            device->AddIPAddress(address, (unsigned int)prefix_length, gateway, ipv6);
            return true;
        }
        catch (NetCfgProxyException& e)
        {
            std::stringstream err;
            err << "Error adding IP address " << address
                << "/" << std::to_string(prefix_length)
                << " to " << device->GetDeviceName()
                << ": " << std::string(e.what());
            signal->LogError(err.str());
            return false;
        }
    }


    bool tun_builder_set_layer(int layer) override
    {
        device->SetLayer(layer);
        return true;
    }


    bool tun_builder_set_mtu(int mtu) override
    {
        device->SetMtu(mtu);
        return true;
    }


    bool tun_builder_reroute_gw(bool ipv4,
                                bool ipv6,
                                unsigned int flags) override
    {
        const std::vector<NetCfgProxy::Network> defaultRoutes;

        /*
         * We add default routes and let the other side figure
         * out the details how to implement this
         * flags are only EmulateExcludeRoutes so far which is not
         * needed on Linux
         */

        if (ipv4)
        {
            device->SetRerouteGw(false, true);
        }
        if (ipv6)
        {
            device->SetRerouteGw(true, true);
        }
        return true;
    }


    bool tun_builder_add_route(const std::string& address,
                               int prefix_length,
                               int metric,
                               bool ipv6) override
    {
        /* We ignore metric */
        /* Instead calling the proxy for each network individually we collect them
         * to save context switching/rpc overhead */
        networks.emplace_back(NetCfgProxy::Network(address, prefix_length, ipv6));
        return true;
    }


    bool tun_builder_exclude_route(const std::string& address,
                                   int prefix_length,
                                   int metric,
                                   bool ipv6) override
    {
        networks.emplace_back(NetCfgProxy::Network(address,
                                                   (unsigned int) prefix_length,
                                                   ipv6, true));
        return true;
    }


    bool tun_builder_add_dns_server(const std::string& address, bool ipv6) override
    {
        if (disabled_dns_config)
        {
            return true;
        }

        const std::vector<std::string> dnsserver{{address}};
        device->AddDNS(dnsserver);
        return true;
    }


    bool tun_builder_add_search_domain(const std::string& domain) override
    {
        if (disabled_dns_config)
        {
            return true;
        }

        const std::vector<std::string> dnsdomain{{domain}};
        device->AddDNSSearch(dnsdomain);
        return true;
    }


    int tun_builder_establish() override
    {
        // Set all routes in one go to avoid calling the function multiple
        // times
        device->AddNetworks(networks);
        return device->Establish();
    }


    bool tun_builder_set_block_ipv6(bool block_ipv6) override
    {
        // TODO
        return false;
    }


    void tun_builder_teardown(bool disconnect) override
    {
        if (!device)
        {
            return;
        }

        if (disconnect)
        {
            device->Destroy();
            device.reset(nullptr);
        }
        else
        {
           device->Disable();
        }
    }


    /* We allow this device to be used as persistent device */
    bool tun_builder_persist() override
    {
        return true;
    }


    bool socket_protect(int socket, std::string remote, bool ipv6) override
    {
        return netcfgmgr.ProtectSocket(socket, remote, ipv6);
    }


    bool tun_builder_set_session_name(const std::string& name) override
    {
        signal->LogVerb2("Session name: '" + name + "'");
        return true;
    }


protected:
    bool disabled_dns_config;


private:
    std::vector<NetCfgProxy::Network> networks;
    NetCfgProxy::Device::Ptr device;
    NetCfgProxy::Manager netcfgmgr;
    BackendSignals *signal;
};
