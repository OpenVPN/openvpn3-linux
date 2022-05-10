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

    NetCfgTunBuilder(GDBusConnection* dbuscon, BackendSignals *signal,
                     const std::string& session_token)
        : disabled_dns_config(false),
          netcfgmgr(dbuscon),
          signal(signal),
          session_token(session_token),
          session_name("")
    {
    }

#ifdef OPENVPN3_CORE_CLI_TEST
    NetCfgTunBuilder(GDBusConnection* dbuscon)
        : disabled_dns_config(false),
          netcfgmgr(dbuscon)
    {
        signal = new BackendSignals(dbuscon, LogGroup::CLIENT,
                                    "(netcfg-cli-test)",
                                    nullptr);
        signal->SetLogLevel(6);
        signal->EnableBroadcast(true);
    }
#endif

    ~NetCfgTunBuilder()
    {
        // Explicitly call cleanup
        try
        {
            if (device)
            {
                device->Destroy();
            }
        }
        catch (const NetCfgProxyException& excp)
        {
            signal->LogCritical("Removing device failed: " + excp.GetError());
        }

        try
        {
            netcfgmgr.Cleanup();
        }
        catch (const NetCfgProxyException& excp)
        {
            signal->LogCritical("Cleaning up NetCfgMgr error: " + excp.GetError());
        }
#ifdef OPENVPN3_CORE_CLI_TEST
        delete signal;
#endif
    }

    bool tun_builder_new() override
    {
        // Cleanup the old things
        tun_builder_teardown(true);
        networks.clear();

        return create_device();
    }


    bool tun_builder_set_remote_address(const std::string& address, bool ipv6) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }
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
            if (!device)
            {
                throw NetCfgProxyException(__func__, "Lost link to device interface");
            }
            device->AddIPAddress(address, (unsigned int)prefix_length, gateway, ipv6);
            return true;
        }
        catch (NetCfgProxyException& e)
        {
            std::stringstream err;
            err << "Error adding IP address " << address
                << "/" << std::to_string(prefix_length)
                << " to " << (device ? device->GetDeviceName() : "[unkown]")
                << ": " << std::string(e.what());
            signal->LogError(err.str());
            return false;
        }
    }


    bool tun_builder_set_layer(int layer) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }
        device->SetLayer(layer);
        return true;
    }


    bool tun_builder_set_mtu(int mtu) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }
        device->SetMtu(mtu);
        return true;
    }


    bool tun_builder_reroute_gw(bool ipv4,
                                bool ipv6,
                                unsigned int flags) override
    {
        const std::vector<NetCfgProxy::Network> defaultRoutes;

        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

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
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
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
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

        const std::vector<std::string> dnsdomain{{domain}};
        device->AddDNSSearch(dnsdomain);
        return true;
    }


    int tun_builder_establish() override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

        // Set all routes in one go to avoid calling the function multiple
        // times
        device->AddNetworks(networks);

        int ret = -1;
        try
        {
            ret = device->Establish();
        }
        catch (const DBusProxyAccessDeniedException& excp)
        {
            signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_FAILED);
            try
            {
                tun_builder_teardown(true);
            }
            catch (...)
            {
            }
            signal->LogFATAL("Access denied calling NetCfgDevice::Establish(): " +
                                std::string(excp.what()));
        }
        catch (const DBusException& excp)
        {
            signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_FAILED);
            try
            {
                tun_builder_teardown(true);
            }
            catch (...)
            {
            }
            signal->LogFATAL("Error calling NetCfgDevice::Establish(): " +
                                std::string(excp.what()));
        }
        return ret;
    }


    bool tun_builder_set_allow_family(int af, bool block_ipv6) override
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

#ifdef ENABLE_OVPNDCO
        dco.reset();
#endif

        if (disconnect)
        {
            try
            {
                device->Destroy();
            }
            catch(const DBusException& excp)
            {
                signal->LogCritical("tun_builder_teardown: "
                                    + std::string(excp.GetRawError()));
            }
            device.reset(nullptr);
        }
        else
        {
            try
            {
                device->Disable();
            }
            catch(const DBusException& excp)
            {
                signal->LogCritical(excp.GetRawError());
            }
        }
    }


    /* We allow this device to be used as persistent device */
    bool tun_builder_persist() override
    {
        return true;
    }

    bool socket_protect(int socket, std::string remote, bool ipv6) override
    {
        std::string devpath = "/"; // Object paths cannot be empty
        if (device)
        {
            devpath = device->GetProxyPath();
        }
        return netcfgmgr.ProtectSocket(socket, remote, ipv6, devpath);
    }

    bool tun_builder_set_session_name(const std::string& name) override
    {
        signal->LogVerb2("Session name: '" + name + "'");
        session_name = std::string(name);
        return true;
    }


    std::string tun_builder_get_session_name()
    {
        return session_name;
    }


    std::string netcfg_get_device_path()
    {
        return (device ? device->GetProxyPath() : "");
    }


    std::string netcfg_get_device_name()
    {
        return (device ? device->GetDeviceName() : "");
    }

#ifdef ENABLE_OVPNDCO
    bool tun_builder_dco_available() override
    {
        return netcfgmgr.DcoAvailable();
    }

    int tun_builder_dco_enable(const std::string& dev_name) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__,
                                       "Lost link to device interface");
        }

        try
        {
            if (!dco)
            {
                dco.reset(device->EnableDCO(dev_name));
            }
            return dco->GetPipeFD();
        }
        catch(const std::exception& e)
        {
            signal->LogError(e.what());
            return -1;
        }
    }

    virtual void tun_builder_dco_new_peer(uint32_t peer_id, uint32_t transport_fd, struct sockaddr *sa,
                                          socklen_t salen, IPv4::Addr& vpn4, IPv6::Addr& vpn6) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->NewPeer(peer_id, transport_fd, sa, salen, vpn4, vpn6);
    }

    void tun_builder_dco_new_key(unsigned int key_slot, const KoRekey::KeyConfig* kc) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->NewKey(key_slot, kc);
    }

    void tun_builder_dco_establish() override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        device->AddNetworks(networks);
        device->EstablishDCO();
    }

    void tun_builder_dco_swap_keys(uint32_t peer_id) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->SwapKeys(peer_id);
    }

    void tun_builder_dco_set_peer(uint32_t peer_id, int keepalive_interval, int keepalive_timeout) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->SetPeer(peer_id, keepalive_interval, keepalive_timeout);
    }
#endif  // ENABLE_OVPNDCO

protected:
    bool disabled_dns_config;
    std::string dns_scope = "global";


private:
    bool create_device()
    {
        if (device)
            return true;

        try
        {
            std::string devpath = netcfgmgr.CreateVirtualInterface(session_token);
            device.reset(netcfgmgr.getVirtualInterface(devpath));
            try
            {
                device->SetProperty("dns_scope", dns_scope);
            }
            catch(const DBusException& excp)
            {
                signal->LogCritical("Failed changing DNS Scope: "
                                    + std::string(excp.GetRawError()));
            }
            return true;
        }
        catch (NetCfgProxyException& e)
        {
            signal->LogError(std::string("Error creating virtual network device: ") + e.what());
            return false;
        }
    }

    std::vector<NetCfgProxy::Network> networks;
    NetCfgProxy::Device::Ptr device;
#ifdef ENABLE_OVPNDCO
    NetCfgProxy::DCO::Ptr dco;
#endif
    NetCfgProxy::Manager netcfgmgr;
    BackendSignals *signal;
    std::string session_token;
    std::string session_name;
};
