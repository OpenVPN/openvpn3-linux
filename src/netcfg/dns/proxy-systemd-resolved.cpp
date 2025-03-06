//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   proxy-systemd-resolved.cpp
 *
 * @brief  D-Bus proxy for the systemd-resolved service
 */

#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include <openvpn/common/split.hpp>

#include "netcfg/dns/proxy-systemd-resolved.hpp"
#include "netcfg/dns/systemd-resolved-exception.hpp"

using namespace NetCfg::DNS::resolved;


namespace NetCfg {
namespace DNS {
namespace resolved {

namespace Error {

std::mutex access_mtx;

Message::Message(const std::string &method_,
                 const std::string &message_)
    : method(method_), message(message_)
{
}


std::string Message::str() const
{
    return "[" + method + "] " + message;
}

Storage::Ptr Storage::Create()
{
    return Storage::Ptr(new Storage());
}

Storage::Storage() = default;
Storage::~Storage() noexcept = default;


void Storage::Add(const std::string &link, const std::string &method, const std::string &message)
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    errors_[link].emplace_back(method, message);
}


std::vector<std::string> Storage::GetLinks() const
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    std::vector<std::string> ret;
    for (const auto &[link, err] : errors_)
    {
        ret.push_back(link);
    }
    return ret;
}


size_t Storage::NumErrors(const std::string &link) const
{
    auto f = errors_.find(link);
    return (f != errors_.end() ? f->second.size() : 0);
}


Error::Message::List Storage::GetErrors(const std::string &link)
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    auto recs = errors_.extract(link);
    return !recs.empty() ? recs.mapped() : Error::Message::List{};
}

} // namespace Error



//
//  NetCfg::DNS::resolved::SearchDomain
//
SearchDomain::SearchDomain(const std::string &srch, const bool rout)
    : search(std::move(srch)), routing(rout)
{
}


SearchDomain::SearchDomain(GVariant *entry)
{
    glib2::Utils::checkParams(__func__, entry, "(sb)", 2);

    search = glib2::Value::Extract<std::string>(entry, 0);
    routing = glib2::Value::Extract<bool>(entry, 1);
}


GVariant *SearchDomain::GetGVariant() const
{
    if (search.empty())
    {
        return nullptr;
    }
    GVariantBuilder *b = glib2::Builder::Create("(sb)");
    glib2::Builder::Add(b, search);
    glib2::Builder::Add(b, routing);
    return glib2::Builder::Finish(b);
}



//
//  NetCfg::DNS::resolved::Link
//

Link::Ptr Link::Create(DBus::Proxy::Client::Ptr prx,
                       const DBus::Object::Path &path,
                       const std::string &devname)
{
    return Link::Ptr(new Link(prx, path, devname));
}


Link::Link(DBus::Proxy::Client::Ptr prx,
           const DBus::Object::Path &path,
           const std::string &devname)
    : proxy(prx), device_name(devname)
{
    tgt_link = DBus::Proxy::TargetPreset::Create(path,
                                                 "org.freedesktop.resolve1.Link");
}


const DBus::Object::Path Link::GetPath() const
{
    return (tgt_link ? tgt_link->object_path : "");
}


std::string Link::GetDeviceName() const
{
    return device_name;
}


const std::vector<std::string> Link::GetDNSServers() const
{
    GVariant *r = proxy->GetPropertyGVariant(tgt_link, "DNS");
    glib2::Utils::checkParams(__func__, r, "a(iay)");


    GVariantIter *it = g_variant_iter_new(r);

    GVariant *rec = nullptr;
    std::vector<std::string> dns_srvs;
    while ((rec = g_variant_iter_next_value(it)))
    {
        IPAddress d(rec);
        dns_srvs.push_back(d.str());
        g_variant_unref(rec);
    }
    g_variant_iter_free(it);
    g_variant_unref(r);

    return dns_srvs;
}


std::vector<std::string> Link::SetDNSServers(const IPAddress::List &servers) const
{
    GVariantBuilder *b = glib2::Builder::Create("a(iay)");
    std::vector<std::string> applied{};
    for (const auto &srv : servers)
    {
        glib2::Builder::Add(b, srv.GetGVariant());
        applied.push_back(srv.str());
    }

    GVariant *r = proxy->Call(tgt_link,
                              "SetDNS",
                              glib2::Builder::FinishWrapped(b));
    g_variant_unref(r);
    return applied;
}


const std::string Link::GetCurrentDNSServer() const
{
    GVariant *r = nullptr;
    try
    {
        r = proxy->GetPropertyGVariant(tgt_link, "CurrentDNSServer");
        IPAddress d(r);
        g_variant_unref(r);
        return d.str();
    }
    catch (const Exception &)
    {
        // Ignore exceptions and instead return an empty server
        // in this case
        g_variant_unref(r);
        return "";
    }
    catch (const DBus::Exception &)
    {
        return "";
    }
}


const SearchDomain::List Link::GetDomains() const
{
    GVariant *r = proxy->GetPropertyGVariant(tgt_link, "Domains");
    glib2::Utils::checkParams(__func__, r, "a(sb)");

    GVariantIter *it = g_variant_iter_new(r);
    SearchDomain::List ret{};
    GVariant *el = nullptr;
    while ((el = g_variant_iter_next_value(it)))
    {
        SearchDomain dom(el);
        ret.push_back(dom);
        g_variant_unref(el);
    }
    g_variant_iter_free(it);
    g_variant_unref(r);

    return ret;
}


std::vector<std::string> Link::SetDomains(const SearchDomain::List &doms) const
{
    GVariantBuilder *b = glib2::Builder::Create("a(sb)");
    std::vector<std::string> applied{};
    for (const auto &dom : doms)
    {
        GVariant *r = dom.GetGVariant();
        if (r)
        {
            g_variant_builder_add_value(b, r);
            applied.push_back(dom.search);
        }
    }

    GVariant *r = proxy->Call(tgt_link,
                              "SetDomains",
                              glib2::Builder::FinishWrapped(b));
    g_variant_unref(r);
    return applied;
}


bool Link::GetDefaultRoute() const
{
    try
    {
        return proxy->GetProperty<bool>(tgt_link, "DefaultRoute");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not extract DefaultRoute");
    }
}


bool Link::SetDefaultRoute(const bool route)
{
    if (!feature_set_default_route)
    {
        return false;
    }
    try
    {
        GVariant *r = proxy->Call(tgt_link,
                                  "SetDefaultRoute",
                                  glib2::Value::CreateTupleWrapped(route));
        g_variant_unref(r);
        return true;
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        std::string err(excp.what());
        if (err.find("GDBus.Error:org.freedesktop.DBus.Error.UnknownMethod") > 0)
        {
            feature_set_default_route = false;
            return false;
        }
        throw excp;
    }
}


std::string Link::GetDNSSEC() const
{
    try
    {
        return proxy->GetProperty<std::string>(tgt_link, "DNSSEC");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not retrieve DNSSEC mode: "
                        + std::string(excp.GetRawError()));
    }
}


void Link::SetDNSSEC(const std::string &mode) const
{
    if (mode != "yes" && mode != "no" && mode != "allow-downgrade")
    {
        throw Exception("Invalid DNSSEC mode requested: " + mode);
    }

    try
    {
        GVariant *r = proxy->Call(tgt_link,
                                  "SetDNSSEC",
                                  glib2::Value::CreateTupleWrapped(mode));
        g_variant_unref(r);
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        throw Exception("Could not set the DNSSEC mode: "
                        + std::string(excp.GetRawError()));
    }
}


std::string Link::GetDNSOverTLS() const
{
    try
    {
        return proxy->GetProperty<std::string>(tgt_link, "DNSOverTLS");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not retrieve DNSOverTLS mode: "
                        + std::string(excp.GetRawError()));
    }
}


void Link::SetDNSOverTLS(const std::string &mode) const
{
    if (mode != "no" && mode != "false"
        && mode != "yes" && mode != "true"
        && mode != "opportunistic")
    {
        throw Exception("Invalid DNSOverTLS mode requested: " + mode);
    }
    try
    {
        GVariant *r = proxy->Call(tgt_link,
                                  "SetDNSOverTLS",
                                  glib2::Value::CreateTupleWrapped(mode));
        g_variant_unref(r);
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        throw Exception("Could not set the DNSOverTLS mode: "
                        + std::string(excp.GetRawError()));
    }
}


void Link::Revert() const
{
    GVariant *r = proxy->Call(tgt_link, "Revert");
    g_variant_unref(r);
}



//
//  NetCfg::DNS::resolved::Manager
//

Manager::Ptr Manager::Create(DBus::Connection::Ptr conn)
{
    return Manager::Ptr(new Manager(conn));
}


Manager::Manager(DBus::Connection::Ptr conn)
{
    proxy = DBus::Proxy::Client::Create(conn, "org.freedesktop.resolve1");
    tgt_resolved = DBus::Proxy::TargetPreset::Create(
        "/org/freedesktop/resolve1", "org.freedesktop.resolve1.Manager");

    // Check for presence of org.freedesktop.PolicyKit1
    // This service is needed to be allowed to send update requests
    // to systemd-resolved as the 'openvpn' user which net.openvpn.v3.netcfg
    // run as
    try
    {
        auto prxsrv = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
        if (prxsrv->StartServiceByName("org.freedesktop.PolicyKit1") < 1)
        {
            throw DBus::Exception(__func__, "");
        }

        std::string n = prxsrv->GetNameOwner("org.freedesktop.PolicyKit1");
        if (n.empty())
        {
            throw DBus::Exception(__func__, "");
        }
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception(std::string("Could not access ")
                        + "org.freedesktop.PolicyKit1 (polkitd) service. "
                        + "Cannot configure systemd-resolved integration");
    }
}


Link::Ptr Manager::RetrieveLink(const std::string &dev_name) const
{
    unsigned int if_idx = ::if_nametoindex(dev_name.c_str());
    if (0 == if_idx)
    {
        std::stringstream err;
        err << "Could not retrieve if_index for '" << dev_name << "': "
            << std::string(::strerror(errno));
        throw Exception(err.str());
    }
    auto link_path = GetLink(if_idx);
    if (link_path.empty())
    {
        return nullptr;
    }
    return Link::Create(proxy, link_path, dev_name);
}


DBus::Object::Path Manager::GetLink(unsigned int if_idx) const
{
    GVariant *res = proxy->Call(tgt_resolved,
                                "GetLink",
                                glib2::Value::CreateTupleWrapped(if_idx, "i"));
    glib2::Utils::checkParams("GetLink", res, "(o)", 1);
    try
    {
        auto link_path = glib2::Value::Extract<DBus::Object::Path>(res, 0);
        g_variant_unref(res);
        return link_path;
    }
    catch (const DBus::Exception &excp)
    {
        std::ostringstream err;
        err << "Could not retrieve systemd-resolved path for "
            << "if_index " + std::to_string(if_idx) + ": "
            << excp.what();
        throw Exception(err.str());
    }
}

} // namespace resolved
} // namespace DNS
} // namespace NetCfg
