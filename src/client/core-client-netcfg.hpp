//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  Antonio Quartulli <antonio@openvpn.net>
//  Copyright (C)  Heiko Hund <heiko@openvpn.net>
//

/**
 * @file   core-client-netcfg.cpp
 *
 * @brief Class that extends the client class and also implements the
 *          tunbuilder methods using NetCfg service
 *
 */

#pragma once

#include "build-config.h"

#include <openvpn/client/dns.hpp>

#include "netcfg/proxy-netcfg-device.hpp"
#include "netcfg/proxy-netcfg-mgr.hpp"
#include "backend-signals.hpp"


template <class T>
class NetCfgTunBuilder : public T
{
  public:
    using Ptr = std::shared_ptr<NetCfgTunBuilder>;

    NetCfgTunBuilder(DBus::Connection::Ptr dbuscon,
                     BackendSignals::Ptr signals_,
                     const std::string &session_token_)
        : disabled_dns_config(false),
          signals(signals_),
          netcfgmgr(NetCfgProxy::Manager::Create(dbuscon)),
          session_token(session_token_),
          session_name("")
    {
    }


#ifdef OPENVPN3_CORE_CLI_TEST
    NetCfgTunBuilder(DBus::Connection::Ptr dbuscon)
        : disabled_dns_config(false),
          netcfgmgr(NetCfgProxy::Manager::Create(dbuscon))
    {
        signals = BackendSignals::Create(dbuscon,
                                         LogGroup::CLIENT,
                                         "(netcfg-cli-test)",
                                         nullptr);
        signals->SetLogLevel(6);
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
        catch (const NetCfgProxyException &excp)
        {
            signals->LogCritical("Removing device failed: " + excp.GetError());
        }

        try
        {
            signals->Debug("Network configuration cleanup requested");
            netcfgmgr->Cleanup();
        }
        catch (const NetCfgProxyException &excp)
        {
            signals->LogCritical("Cleaning up NetCfgMgr error: " + excp.GetError());
        }
    }


    bool tun_builder_new() override
    {
        // Cleanup the old things
        tun_builder_teardown(true);
        networks.clear();

        return create_device();
    }


    bool tun_builder_set_remote_address(const std::string &address, bool ipv6) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }
        try
        {
            device->SetRemoteAddress(address, ipv6);
            return true;
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed saving remote address: "
                << excp.GetRawError();
            signals->LogError(msg.str());
            return false;
        }
    }


    bool tun_builder_add_address(const std::string &address,
                                 int prefix_length,
                                 const std::string &gateway, // optional
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
        catch (const DBus::Proxy::Exception &e)
        {
            std::stringstream err;
            err << "Error adding IP address " << address
                << "/" << std::to_string(prefix_length)
                << " to " << (device ? device->GetDeviceName() : "[unknown]")
                << ": " << std::string(e.what());
            signals->LogError(err.str());
            return false;
        }
    }


    bool tun_builder_set_layer(int layer) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }
        try
        {
            device->SetLayer(layer);
            return true;
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed setting tunnel layer: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
            return false;
        }
    }


    bool tun_builder_set_mtu(int mtu) override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

        try
        {
            device->SetMtu(mtu);
            return true;
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed setting tunnel MTU: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
            return false;
        }
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

        try
        {
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
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed setting gateway re-route flag: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
            return false;
        }
    }


    bool tun_builder_add_route(const std::string &address,
                               int prefix_length,
                               int metric,
                               bool ipv6) override
    {
        /* We ignore metric */
        /* Instead calling the proxy for each network individually we collect them
         * to save context switching/rpc overhead */
        networks.push_back(
            NetCfgProxy::Network::IncludeRoute(address, prefix_length, ipv6));
        return true;
    }


    bool tun_builder_exclude_route(const std::string &address,
                                   int prefix_length,
                                   int metric,
                                   bool ipv6) override
    {
        networks.push_back(
            NetCfgProxy::Network::ExcludeRoute(address, prefix_length, ipv6));
        return true;
    }


    bool tun_builder_set_dns_options(const openvpn::DnsOptions &dns) override
    {
        if (disabled_dns_config)
        {
            signals->LogVerb1("DNS configuration is disabled in the profile, skipping");
            return true;
        }

        return device->AddDnsOptions(signals, dns);
    }


    int tun_builder_establish() override
    {
        if (!device)
        {
            throw NetCfgProxyException(__func__, "Lost link to device interface");
        }

        // Set all routes in one go to avoid calling the function multiple
        // times
        try
        {
            device->AddNetworks(networks);
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed adding networks: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
        }

        try
        {
            return device->Establish();
        }
        catch (const DBus::Exception &excp)
        {
            signals->StatusChange(Events::Status(StatusMajor::CONNECTION, StatusMinor::CONN_FAILED));
            signals->LogFATAL("Error calling NetCfgDevice::Establish(): "
                              + std::string(excp.what()));
            try
            {
                tun_builder_teardown(true);
            }
            catch (...)
            {
            }
            return -1; // Indicate no tun fd available
        }
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
            catch (const DBus::Exception &excp)
            {
                signals->LogCritical("tun_builder_teardown: "
                                     + std::string(excp.GetRawError()));
            }
            device.reset();
        }
        else
        {
            try
            {
                device->Disable();
            }
            catch (const DBus::Exception &excp)
            {
                signals->LogCritical(excp.GetRawError());
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
            devpath = device->GetDevicePath();
        }
        try
        {
            return netcfgmgr->ProtectSocket(socket, remote, ipv6, devpath);
        }
        catch (const DBus::Exception &excp)
        {
            std::ostringstream msg;
            if (device)
            {
                msg << "[" << device->GetDeviceName() << "] ";
            }
            msg << "Failed protecting socket: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
            return false;
        }
    }


    bool tun_builder_set_session_name(const std::string &name) override
    {
        signals->LogVerb2("Session name: '" + name + "'");
        session_name = std::string(name);
        return true;
    }


    std::string tun_builder_get_session_name()
    {
        return session_name;
    }


    DBus::Object::Path netcfg_get_device_path()
    {
        return (device ? device->GetDevicePath() : "");
    }


    std::string netcfg_get_device_name()
    {
        return (device ? device->GetDeviceName() : "");
    }


#ifdef ENABLE_OVPNDCO
    bool tun_builder_dco_available() override
    {
        return netcfgmgr->DcoAvailable();
    }


    int tun_builder_dco_enable(const std::string &dev_name) override
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
        catch (const std::exception &e)
        {
            signals->LogError(e.what());
            return -1;
        }
    }


    virtual void tun_builder_dco_new_peer(uint32_t peer_id,
                                          uint32_t transport_fd,
                                          struct sockaddr *sa,
                                          socklen_t salen,
                                          openvpn::IPv4::Addr &vpn4,
                                          openvpn::IPv6::Addr &vpn6) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->NewPeer(peer_id, transport_fd, sa, salen, vpn4, vpn6);
    }


    void tun_builder_dco_new_key(unsigned int key_slot,
                                 const openvpn::KoRekey::KeyConfig *kc) override
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

        try
        {
            device->AddNetworks(networks);
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            std::ostringstream msg;
            msg << "[" << device->GetDeviceName() << "] "
                << "Failed adding networks: "
                << excp.GetRawError();
            signals->LogCritical(msg.str());
        }

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


    void tun_builder_dco_set_peer(uint32_t peer_id,
                                  int keepalive_interval,
                                  int keepalive_timeout) override
    {
        if (!dco)
        {
            NetCfgProxyException(__func__, "Lost link to DCO device");
        }

        dco->SetPeer(peer_id, keepalive_interval, keepalive_timeout);
    }
#endif // ENABLE_OVPNDCO


  protected:
    bool disabled_dns_config;
    std::string dns_scope = "global";
    BackendSignals::Ptr signals;

  private:
    bool create_device()
    {
        if (device)
            return true;

        try
        {
            std::string devpath = netcfgmgr->CreateVirtualInterface(session_token);
            device.reset(netcfgmgr->getVirtualInterface(devpath));
            try
            {
                device->SetDNSscope(dns_scope);
            }
            catch (const DBus::Exception &excp)
            {
                signals->LogCritical("Failed changing DNS Scope: "
                                     + std::string(excp.GetRawError()));
            }
            return true;
        }
        catch (const std::exception &e)
        {
            signals->LogError(std::string("Error creating virtual network device: ") + e.what());
            return false;
        }
    }

    std::vector<NetCfgProxy::Network> networks;
    NetCfgProxy::Device::Ptr device;
#ifdef ENABLE_OVPNDCO
    NetCfgProxy::DCO::Ptr dco;
#endif
    NetCfgProxy::Manager::Ptr netcfgmgr = nullptr;
    std::string session_token;
    std::string session_name;
};
