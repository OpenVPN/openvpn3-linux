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
 * @file   proxy-netcfg-mgr.hpp
 *
 * @brief  Declaration of the D-Bus proxy for the main manager object
 *         of the net.openvpn.v3.netcfg service
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "netcfg-subscriptions.hpp"


namespace NetCfgProxy {
class Device;

class Manager
{
  public:
    using Ptr = std::shared_ptr<Manager>;

    /**
     *  Initialize the Network Configuration proxy for the
     *  main management interface
     *
     * @param dbuscon  D-Bus connection to use for D-Bus calls
     */
    [[nodiscard]] static Manager::Ptr Create(DBus::Connection::Ptr dbuscon);

    const std::string GetConfigFile();

    const DBus::Object::Path CreateVirtualInterface(const std::string &device_name);

#ifdef OPENVPN3_NETCFGPRX_DEVICE
    Device *getVirtualInterface(const DBus::Object::Path &path)
    {
        return new Device(dbuscon, path);
    }
#endif

    DBus::Object::Path::List FetchInterfaceList();
    bool ProtectSocket(int socket, const std::string &remote, bool ipv6, const std::string &devpath);
    bool DcoAvailable();
    void Cleanup();

    void NotificationSubscribe(NetCfgChangeType filter_flags);
    void NotificationUnsubscribe(const std::string &subscriber);
    void NotificationUnsubscribe();
    NetCfgSubscriptions::NetCfgNotifSubscriptions NotificationSubscriberList();

  private:
    DBus::Connection::Ptr dbuscon{nullptr};
    DBus::Proxy::Client::Ptr proxy{nullptr};
    DBus::Proxy::Utils::Query::Ptr proxy_helper{nullptr};
    DBus::Proxy::TargetPreset::Ptr tgt_mgr{nullptr};

    Manager(DBus::Connection::Ptr dbuscon);
};
} // namespace NetCfgProxy
