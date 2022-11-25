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
 * @file   proxy-netcfg-mgr.cpp
 *
 * @brief  Implementation of D-Bus proxy for the main manager object
 *         of the net.openvpn.v3.netcfg service
 */

#include <string>
#include <vector>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "dbus/glibutils.hpp"
#include "proxy-netcfg-mgr.hpp"
#include "netcfg-exception.hpp"
#include "netcfg-subscriptions.hpp"

using namespace openvpn;


namespace NetCfgProxy {

//
//  class NetCfgProxy::Manager
//

Manager::Manager(GDBusConnection *dbuscon)
    : DBusProxy(dbuscon,
                OpenVPN3DBus_name_netcfg,
                OpenVPN3DBus_interf_netcfg,
                OpenVPN3DBus_rootp_netcfg)
{
    try
    {
        CheckServiceAvail();
        (void)GetServiceVersion();
    }
    catch (const DBusException &)
    {
        throw NetCfgProxyException(
            "Init", "Could not connect to net.openvpn.v3.netcfg service");
    }
}


const std::string Manager::GetConfigFile()
{
    if (!CheckObjectExists())
    {
        throw NetCfgProxyException("GetConfigFile",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    return GetStringProperty("config_file");
}


const std::string Manager::CreateVirtualInterface(const std::string &device_name)
{
    Ping();
    try
    {
        GVariant *res = Call("CreateVirtualInterface",
                             g_variant_new("(s)",
                                           device_name.c_str()));
        if (!res)
        {
            throw NetCfgProxyException("CreateVirtualInterface",
                                       "No results returned");
        }

        gchar *path = nullptr;
        g_variant_get(res, "(o)", &path);
        const std::string devpath(path);
        g_free(path);
        g_variant_unref(res);
        return devpath;
    }
    catch (NetCfgProxyException &)
    {
        throw;
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("CreateVirtualInterface",
                                   excp.what());
    }
}


bool Manager::ProtectSocket(int socket, const std::string &remote, bool ipv6, const std::string &devpath)
{
    if (!CheckObjectExists())
    {
        throw NetCfgProxyException("ProtectSocket",
                                   "net.openvpn.v3.netcfg service unavailable");
    }

    bool ret;
    try
    {
        // If protecting socked fd is disabled, we get
        // a -1 for the socket
        GVariant *res;
        if (socket < 0)
        {
            res = Call("ProtectSocket",
                       g_variant_new("(sbo)",
                                     remote.c_str(),
                                     ipv6,
                                     devpath.c_str()));
        }
        else
        {
            res = CallSendFD("ProtectSocket",
                             g_variant_new("(sbo)",
                                           remote.c_str(),
                                           ipv6,
                                           devpath.c_str()),
                             socket);
        }
        GLibUtils::checkParams(__func__, res, "(b)", 1);
        ret = GLibUtils::GetVariantValue<bool>(g_variant_get_child_value(res, 0));
        g_variant_unref(res);
    }
    catch (NetCfgProxyException &)
    {
        throw;
    }
    return ret;
}


bool Manager::DcoAvailable()
{
    GVariant *res = Call("DcoAvailable");
    GLibUtils::checkParams(__func__, res, "(b)", 1);
    bool ret = GLibUtils::GetVariantValue<bool>(g_variant_get_child_value(res, 0));
    g_variant_unref(res);
    return ret;
}


void Manager::Cleanup()
{
    if (!CheckObjectExists())
    {
        throw NetCfgProxyException("Cleanup",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        Call("Cleanup");
    }
    catch (NetCfgProxyException &)
    {
        throw;
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("Cleanup",
                                   excp.what());
    }
}


std::vector<std::string> Manager::FetchInterfaceList()
{
    Ping();
    try
    {
        GVariant *res = Call("FetchInterfaceList");
        if (!res)
        {
            throw NetCfgProxyException("FetchInterfaceList",
                                       "No results returned");
        }

        GVariantIter *pathlist = nullptr;
        g_variant_get(res, "(ao)", &pathlist);

        GVariant *path = nullptr;
        std::vector<std::string> device_paths;
        while ((path = g_variant_iter_next_value(pathlist)))
        {
            gsize len;
            device_paths.push_back(std::string(g_variant_get_string(path, &len)));
            g_variant_unref(path);
        }
        g_variant_iter_free(pathlist);
        g_variant_unref(res);
        return device_paths;
    }
    catch (NetCfgProxyException &)
    {
        throw;
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("CreateVirtualInterface",
                                   excp.what());
    }
}


void Manager::NotificationSubscribe(NetCfgChangeType filter_flags)
{
    Ping();
    try
    {
        Call("NotificationSubscribe", g_variant_new("(u)", static_cast<std::uint16_t>(filter_flags), true));
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("NotificationSubscribe",
                                   excp.what());
    }
}


void Manager::NotificationUnsubscribe()
{
    NotificationUnsubscribe(std::string());
}


void Manager::NotificationUnsubscribe(const std::string &subscriber)
{
    Ping();
    try
    {
        Call("NotificationUnsubscribe", g_variant_new("(s)", subscriber.c_str()), true);
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("NotificationUnsubscribe",
                                   excp.what());
    }
}


NetCfgSubscriptions::NetCfgNotifSubscriptions Manager::NotificationSubscriberList()
{
    Ping();
    try
    {
        GVariant *res = Call("NotificationSubscriberList");
        if (!res)
        {
            throw NetCfgProxyException("NotificationSubscriberList",
                                       "No results returned");
        }

        GVariantIter *iter = nullptr;
        g_variant_get(res, "(a(su))", &iter);

        GVariant *val = nullptr;
        NetCfgSubscriptions::NetCfgNotifSubscriptions subscriptions;
        while ((val = g_variant_iter_next_value(iter)))
        {
            gchar *dbusname;
            gint filter_bitmask;
            g_variant_get(val, "(su)", &dbusname, &filter_bitmask);
            subscriptions.insert(NetCfgSubscriptions::NetCfgNotifSubscriptions::
                                     value_type(dbusname, filter_bitmask));
            g_free(dbusname);
            g_variant_unref(val);
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
        return subscriptions;
    }
    catch (std::exception &excp)
    {
        throw NetCfgProxyException("NotificationSubscriberList",
                                   excp.what());
    }
}
} // namespace NetCfgProxy
