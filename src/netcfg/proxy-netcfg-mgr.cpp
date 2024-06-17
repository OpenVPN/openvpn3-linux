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
 * @file   proxy-netcfg-mgr.cpp
 *
 * @brief  Implementation of D-Bus proxy for the main manager object
 *         of the net.openvpn.v3.netcfg service
 */

#include <string>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "dbus/constants.hpp"
#include "proxy-netcfg-mgr.hpp"
#include "netcfg-exception.hpp"
#include "netcfg-subscriptions.hpp"


namespace NetCfgProxy {

//
//  class NetCfgProxy::Manager
//

Manager::Ptr Manager::Create(DBus::Connection::Ptr dbuscon)
{
    return Manager::Ptr(new Manager(dbuscon));
}


Manager::Manager(DBus::Connection::Ptr dbuscon_)
    : dbuscon(dbuscon_)
{
    try
    {
        auto srvc = DBus::Proxy::Utils::DBusServiceQuery::Create(dbuscon);
        if (!srvc->CheckServiceAvail(Constants::GenServiceName("netcfg")))
        {
            throw NetCfgProxyException("Init",
                                       "Could not reach "
                                           + Constants::GenServiceName("netcfg"));
        }
        proxy = DBus::Proxy::Client::Create(dbuscon, Constants::GenServiceName("netcfg"));
        tgt_mgr = DBus::Proxy::TargetPreset::Create(Constants::GenPath("netcfg"),
                                                    Constants::GenInterface("netcfg"));
        proxy_helper = DBus::Proxy::Utils::Query::Create(proxy);
        (void)proxy_helper->ServiceVersion(tgt_mgr->object_path,
                                           tgt_mgr->interface);
    }
    catch (const DBus::Exception &)
    {
        throw NetCfgProxyException(
            "Init", "Could not connect to net.openvpn.v3.netcfg service");
    }
}


const std::string Manager::GetConfigFile()
{
    if (!proxy_helper->CheckObjectExists(tgt_mgr->object_path,
                                         tgt_mgr->interface))
    {
        throw NetCfgProxyException("GetConfigFile",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    return proxy->GetProperty<std::string>(tgt_mgr, "config_file");
}


const DBus::Object::Path Manager::CreateVirtualInterface(const std::string &device_name)
{
    proxy_helper->Ping();
    try
    {
        GVariant *res = proxy->Call(tgt_mgr,
                                    "CreateVirtualInterface",
                                    glib2::Value::CreateTupleWrapped(device_name));
        glib2::Utils::checkParams(__func__, res, "(o)");
        const auto devpath = glib2::Value::Extract<DBus::Object::Path>(res, 0);
        g_variant_unref(res);
        return devpath;
    }
    catch (const DBus::Exception &excp)
    {
        throw NetCfgProxyException("CreateVirtualInterface",
                                   excp.GetRawError());
    }
}


bool Manager::ProtectSocket(int socket, const std::string &remote, bool ipv6, const std::string &devpath)
{
    if (!proxy_helper->CheckObjectExists(tgt_mgr->object_path,
                                         tgt_mgr->interface))
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
            res = proxy->Call(tgt_mgr,
                              "ProtectSocket",
                              g_variant_new("(sbo)", remote.c_str(), ipv6, devpath.c_str()));
        }
        else
        {
            res = proxy->SendFD(tgt_mgr,
                                "ProtectSocket",
                                g_variant_new("(sbo)",
                                              remote.c_str(),
                                              ipv6,
                                              devpath.c_str()),
                                socket);
        }
        glib2::Utils::checkParams(__func__, res, "(b)", 1);
        ret = glib2::Value::Extract<bool>(res, 0);
        g_variant_unref(res);
        return ret;
    }
    catch (const DBus::Exception &)
    {
        throw;
    }
}


bool Manager::DcoAvailable()
{
    GVariant *res = proxy->Call(tgt_mgr, "DcoAvailable");
    glib2::Utils::checkParams(__func__, res, "(b)", 1);
    bool ret = glib2::Value::Extract<bool>(res, 0);
    g_variant_unref(res);
    return ret;
}


void Manager::Cleanup()
{
    if (!proxy_helper->CheckObjectExists(tgt_mgr->object_path,
                                         tgt_mgr->interface))
    {
        throw NetCfgProxyException("Cleanup",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        GVariant *res = proxy->Call(tgt_mgr, "Cleanup");
        if (res)
        {
            g_variant_unref(res);
        }
    }
    catch (const DBus::Exception &excp)
    {
        throw NetCfgProxyException("Cleanup",
                                   excp.GetRawError());
    }
}


DBus::Object::Path::List Manager::FetchInterfaceList()
{
    if (!proxy_helper->Ping())
    {
        throw NetCfgProxyException("FetchInterfaceList",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        GVariant *res = proxy->Call(tgt_mgr, "FetchInterfaceList");
        auto device_paths = glib2::Value::ExtractVector<DBus::Object::Path>(res);
        return device_paths;
    }
    catch (const DBus::Exception &excp)
    {
        throw NetCfgProxyException("FetchInterfaceList",
                                   excp.GetRawError());
    }
}


void Manager::NotificationSubscribe(NetCfgChangeType filter_flags)
{
    if (!proxy_helper->Ping())
    {
        throw NetCfgProxyException("NotificationSubscribe",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        GVariant *r = proxy->Call(tgt_mgr,
                                  "NotificationSubscribe",
                                  glib2::Value::CreateTupleWrapped(filter_flags));
        if (r)
        {
            g_variant_unref(r);
        }
    }
    catch (const DBus::Exception &excp)
    {
        throw NetCfgProxyException("NotificationSubscribe",
                                   excp.GetRawError());
    }
}


void Manager::NotificationUnsubscribe()
{
    NotificationUnsubscribe(std::string());
}


void Manager::NotificationUnsubscribe(const std::string &subscriber)
{
    if (!proxy_helper->Ping())
    {
        throw NetCfgProxyException("NotificationUnsubscribe",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        // This can't be an asynchronous call - the calling process might
        // have exited before the netcfg service processes this call.
        GVariant *r = proxy->Call(tgt_mgr,
                                  "NotificationUnsubscribe",
                                  glib2::Value::CreateTupleWrapped(subscriber));
        if (r)
        {
            g_variant_unref(r);
        }
    }
    catch (const DBus::Exception &excp)
    {

        throw NetCfgProxyException("NotificationUnsubscribe",
                                   excp.GetRawError());
    }
}


NetCfgSubscriptions::NetCfgNotifSubscriptions Manager::NotificationSubscriberList()
{
    if (!proxy_helper->Ping())
    {
        throw NetCfgProxyException("NotificationSubscriberList",
                                   "net.openvpn.v3.netcfg service unavailable");
    }
    try
    {
        GVariant *res = proxy->Call(tgt_mgr, "NotificationSubscriberList");
        glib2::Utils::checkParams(__func__, res, "(a(su))");

        GVariantIter *iter = nullptr;
        g_variant_get(res, "(a(su))", &iter);

        GVariant *val = nullptr;
        NetCfgSubscriptions::NetCfgNotifSubscriptions subscriptions;
        while ((val = g_variant_iter_next_value(iter)))
        {
            auto busname = glib2::Value::Extract<std::string>(val, 0);
            auto filter_mask = glib2::Value::Extract<uint32_t>(val, 1);

            subscriptions.insert(NetCfgSubscriptions::NetCfgNotifSubscriptions::
                                     value_type(busname, filter_mask));
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
        return subscriptions;
    }
    catch (const DBus::Exception &excp)
    {
        throw NetCfgProxyException("NotificationSubscriberList",
                                   excp.GetRawError());
    }
}
} // namespace NetCfgProxy
