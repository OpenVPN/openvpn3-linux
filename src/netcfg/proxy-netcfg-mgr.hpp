//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2022  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019 - 2022  Lev Stipakov <lev@openvpn.net>
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
 * @file   proxy-netcfg-mgr.hpp
 *
 * @brief  Declaration of the D-Bus proxy for the main manager object
 *         of the net.openvpn.v3.netcfg service
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-exception.hpp"
#include "netcfg-subscriptions.hpp"

using namespace openvpn;


namespace NetCfgProxy {
class Device;

class Manager : public DBusProxy
{
  public:
    using Ptr = std::shared_ptr<Manager>;

    /**
     *  Initialize the Network Configuration proxy for the
     *  main management interface
     *
     * @param dbuscon  D-Bus connection to use for D-Bus calls
     */
    Manager(GDBusConnection *dbuscon);

    const std::string GetConfigFile();

    const std::string CreateVirtualInterface(const std::string &device_name);

#ifdef OPENVPN3_NETCFGPRX_DEVICE
    Device *getVirtualInterface(const std::string &path)
    {
        return new Device(GetConnection(), path);
    }
#endif

    std::vector<std::string> FetchInterfaceList();
    bool ProtectSocket(int socket, const std::string &remote, bool ipv6, const std::string &devpath);
    bool DcoAvailable();
    void Cleanup();

    void NotificationSubscribe(NetCfgChangeType filter_flags);
    void NotificationUnsubscribe(const std::string &subscriber);
    void NotificationUnsubscribe();
    NetCfgSubscriptions::NetCfgNotifSubscriptions NotificationSubscriberList();
};
} // namespace NetCfgProxy
