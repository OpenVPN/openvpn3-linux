//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2021-  Antonio Quartulli <antonio@openvpn.net>
//

/**
 * @file   netcfg-device.hpp
 *
 * @brief  D-Bus object representing a single virtual network device
 *         the net.openvpn.v3.netcfg service manages
 */

#pragma once

#include "build-config.h"

#include <functional>
#include <gio/gunixfdlist.h>
#include <gio/gunixconnection.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/object/manager.hpp>

#include <openvpn/common/rc.hpp>

#include "common/lookup.hpp"
#include "core-tunbuilder.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/settings-manager.hpp"
#include "netcfg-options.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-signals.hpp"
#include "netcfg-subscriptions.hpp"

#ifdef ENABLE_OVPNDCO
#include "netcfg-dco.hpp"
#endif

using namespace openvpn;
using namespace NetCfg;


enum NetCfgDeviceType : unsigned int
{
    UNSET = 0, // Primarily to avoid 0 but still have 0 defined
    TAP = 2,
    TUN = 3
    // Expliclity use 3 for tun and 2 for tap as 2==TUN would be very
    // confusing
};



class IPAddr
{
  public:
    IPAddr() = default;
    IPAddr(const std::string &ipaddr, const bool ipv6)
        : address(ipaddr), ipv6(ipv6)
    {
    }

    std::string address;
    bool ipv6;
};



/**
 * Class representing a IPv4 or IPv6 network
 */
class Network : public IPAddr
{
  public:
    Network()
        : IPAddr()
    {
    }

    Network(const std::string &networkAddress,
            const unsigned int prefix,
            const bool ipv6,
            const bool exclude = false)
        : IPAddr(networkAddress, ipv6),
          prefix(prefix), exclude(exclude)
    {
    }


    std::string str() const
    {
        return address + "/" + std::to_string(prefix);
    }
    unsigned int prefix;
    bool exclude;
};



class VPNAddress : public Network
{
  public:
    VPNAddress()
        : Network()
    {
    }

    VPNAddress(const std::string &networkAddress,
               const unsigned int prefix,
               const std::string &gateway,
               const bool ipv6)
        : Network(networkAddress, prefix, ipv6, false),
          gateway(gateway)
    {
    }

    std::string gateway;
};



class NetCfgDevice : public DBus::Object::Base
{
    friend CoreTunbuilderImpl;

  public:
    using Ptr = std::shared_ptr<NetCfgDevice>;

    NetCfgDevice(DBus::Connection::Ptr dbuscon,
                 DBus::Object::Manager::Ptr obj_mgr,
                 const uid_t creator_,
                 const pid_t creator_pid_,
                 const std::string &objpath,
                 const std::string &devname,
                 DNS::SettingsManager::Ptr resolver,
                 NetCfgSubscriptions::Ptr subscriptions,
                 const unsigned int log_level,
                 LogWriter *logwr,
                 const NetCfgOptions &options);
    ~NetCfgDevice() noexcept;

    const bool Authorize(const DBus::Authz::Request::Ptr request) override;

    std::string get_device_name() const noexcept;

    /**
     * Return the pid of the process that created this device
     *
     * @return pid of the process that created this object
     */
    pid_t getCreatorPID();


  protected:
    void set_device_name(const std::string &devnam) noexcept;


  private:
    DBus::Connection::Ptr dbuscon = nullptr;
    DBus::Object::Manager::Ptr object_manager = nullptr;
    openvpn::RCPtr<CoreTunbuilder> tunimpl;
    std::string device_name{};
    uint16_t mtu{1500};
    uint16_t txqueuelen{0};
    pid_t creator_pid{-1};
    DNS::SettingsManager::Ptr resolver;
    bool modified = false;
    NetCfgOptions options;
    unsigned int device_type{NetCfgDeviceType::UNSET};
    std::vector<VPNAddress> vpnips{};
    std::vector<Network> networks{};
    DNS::ResolverSettings::Ptr dnsconfig{nullptr};
    NetCfgSignals::Ptr signals{nullptr};
    IPAddr remote{};
    bool reroute_ipv4{false};
    bool reroute_ipv6{false};
#ifdef ENABLE_OVPNDCO
    NetCfgDCO::Ptr dco_device = nullptr;
#endif

    void method_add_ip_address(GVariant *params);
    void method_set_remote_addr(GVariant *params);
    void method_add_networks(GVariant *params);
    void method_add_dns(GVariant *params);
    void method_add_dns_search(GVariant *params);
    void method_enable_dco(DBus::Object::Method::Arguments::Ptr args);
    void method_establish(DBus::Object::Method::Arguments::Ptr args);
    void method_disable();
    void method_destroy(DBus::Object::Method::Arguments::Ptr);
};
