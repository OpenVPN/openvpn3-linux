//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   proxy-systemd-resolved.cpp
 *
 * @brief  D-Bus proxy for the systemd-resolved service
 */

#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>

#include <openvpn/common/split.hpp>

#include "dbus/core.hpp"
#include "dbus/glibutils.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"

using namespace openvpn;
using namespace NetCfg::DNS::resolved;

namespace NetCfg {
namespace DNS {
namespace resolved {

    //
    //  NetCfg::DNS::resolved::ResolverRecord
    //
    ResolverRecord::ResolverRecord(unsigned short f, std::string s)
        : family(f), server(s)
    {
    }


    ResolverRecord::ResolverRecord(GVariant* entry)
    {
        GLibUtils::checkParams(__func__, entry, "(iay)", 2);

        family = GLibUtils::ExtractValue<int>(entry, 0);
        if (AF_INET != family && AF_INET6 != family)
        {
            throw Exception("Unsupported address family");
        }

        GVariantIter* it = g_variant_iter_new(g_variant_get_child_value(entry, 1));
        std::stringstream s;

        GVariant* el = nullptr;
        bool first = true;
        while ((el = g_variant_iter_next_value(it)))
        {
            if (!first)
            {
                s << ".";
            }
            first = false;
            s << std::to_string(g_variant_get_byte(el));
            g_variant_unref(el);
        }
        g_variant_iter_free(it);

        server = std::string(s.str());
    }


    GVariant* ResolverRecord::GetGVariant() const
    {
        const GVariantType* t = G_VARIANT_TYPE("(iay)");
        GVariantBuilder* b = g_variant_builder_new(t);

        g_variant_builder_add(b, "i", family);

        std::vector<std::string> ip;
        ip = Split::by_char<std::vector<std::string>, NullLex, Split::NullLimit>(
                        server, '.');
        GVariantBuilder* ip_b = g_variant_builder_new(G_VARIANT_TYPE("ay"));
        for (const auto& e : ip)
        {
            g_variant_builder_add(ip_b, "y", std::stoi(e));
        }
        g_variant_builder_add_value(b, g_variant_builder_end(ip_b));

        GVariant* ret = g_variant_builder_end(b);
        g_variant_builder_unref(ip_b);
        g_variant_builder_unref(b);

        return ret;
    }


    //
    //  NetCfg::DNS::resolved::SearchDomain
    //
    SearchDomain::SearchDomain(std::string srch, bool rout)
        : search(srch), routing(rout)
    {
    }


    SearchDomain::SearchDomain(GVariant *entry)
    {
        GLibUtils::checkParams(__func__, entry, "(sb)", 2);

        search = GLibUtils::ExtractValue<std::string>(entry, 0);
        routing = GLibUtils::ExtractValue<bool>(entry, 1);
    }


    GVariant* SearchDomain::GetGVariant() const
    {
        GVariantBuilder* b = g_variant_builder_new(G_VARIANT_TYPE("(sb)"));
        g_variant_builder_add(b, "s", search.c_str());
        g_variant_builder_add(b, "b", routing);

        GVariant *ret = g_variant_builder_end(b);
        g_variant_builder_unref(b);
        return ret;
    }


    //
    //  NetCfg::DNS::resolved::Link
    //

    Link::Link(GDBusConnection* dbuscon, const std::string path)
        : DBusProxy(dbuscon, "org.freedesktop.resolve1",
                    "org.freedesktop.resolve1.Link",
                    path)
    {
    }

    const std::string Link::GetPath() const
    {
        return GetProxyPath();
    }




    const std::vector<std::string> Link::GetDNSServers() const
    {
        GVariant* r = GetProperty("DNS");
        GLibUtils::checkParams(__func__, r, "a(iay)", 0);
        GVariantIter* it = g_variant_iter_new(r);

        GVariant* rec = nullptr;
        std::vector<std::string> dns_srvs;
        while ((rec = g_variant_iter_next_value(it)))
        {
            struct ResolverRecord d(rec);
            dns_srvs.push_back(d.server);
            g_variant_unref(rec);
        }
        g_variant_iter_free(it);
        g_variant_unref(r);

        return dns_srvs;
    }


    void Link::SetDNSServers(const ResolverRecord::List& servers) const
    {
        GVariantBuilder* b = g_variant_builder_new(G_VARIANT_TYPE("a(iay)"));
        for (const auto& srv : servers)
        {
            g_variant_builder_add_value(b, srv.GetGVariant());
        }

        GVariant* r = Call("SetDNS", GLibUtils::wrapInTuple(b));
        g_variant_unref(r);
    }

    const std::string Link::GetCurrentDNSServer() const
    {
        GVariant* r = nullptr;
        try
        {
            r = GetProperty("CurrentDNSServer");
            struct ResolverRecord d(r);
            g_variant_unref(r);
            return d.server;
        }
        catch (const Exception&)
        {
            // Ignore exceptions and instead return an empty server
            // in this case
            g_variant_unref(r);
            return "";
        }
        catch (const DBusException&)
        {
            return "";
        }
    }


    const SearchDomain::List Link::GetDomains() const
    {
        GVariant* r = GetProperty("Domains");
        GLibUtils::checkParams(__func__, r, "a(sb)", 0);

        GVariantIter* it = g_variant_iter_new(r);
        SearchDomain::List ret{};
        GVariant* el = nullptr;
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


    void Link::SetDomains(const SearchDomain::List& doms) const
    {
        GVariantBuilder* b = g_variant_builder_new(G_VARIANT_TYPE("a(sb)"));
        for (const auto& dom : doms)
        {
            g_variant_builder_add_value(b, dom.GetGVariant());
        }

        GVariant* r = Call("SetDomains", GLibUtils::wrapInTuple(b));
        g_variant_unref(r);
    }


    bool Link::GetDefaultRoute() const
    {
        try
        {
            GVariant* r = GetProperty("DefaultRoute");
            GLibUtils::checkParams(__func__, r, "b", 0);
            bool ret = g_variant_get_boolean(r);
            g_variant_unref(r);
            return ret;
        }
        catch (const DBusException& excp)
        {
            throw Exception("Could not extract DefaultRoute");
        }
    }


    void Link::SetDefaultRoute(const bool route) const
    {
        GVariantBuilder* b = g_variant_builder_new(G_VARIANT_TYPE("(b)"));
        g_variant_builder_add_value(b, g_variant_new_boolean(route));
        GVariant* r = Call("SetDefaultRoute", g_variant_builder_end(b));
        g_variant_builder_unref(b);
        g_variant_unref(r);
    }

    void Link::Revert() const
    {
        GVariant* r = Call("Revert");
        g_variant_unref(r);
    }


    //
    //  NetCfg::DNS::resolved::Manager
    //

    Manager::Manager(GDBusConnection* dbuscon)
        : DBusProxy(dbuscon, "org.freedesktop.resolve1",
                    "org.freedesktop.resolve1.Manager",
                    "/org/freedesktop/resolve1")
    {
        // Check for presence of org.freedesktop.PolicyKit1
        // This service is needed to be allowed to send update requests
        // to systemd-resolved as the 'openvpn' user which net.openvpn.v3.netcfg
        // run as
        try
        {
            (void) StartServiceByName("org.freedesktop.PolicyKit1");
            (void) GetNameOwner("org.freedesktop.PolicyKit1");
        }
        catch (const DBusException& excp)
        {
            throw Exception(std::string("Could not access ")
                                  + "org.freedesktop.PolicyKit1 (polkitd) service. "
                                  + "Cannot configure systemd-resolved integration");
        }
    }


    Link::Ptr Manager::RetrieveLink(const std::string dev_name)
    {
        unsigned int if_idx = ::if_nametoindex(dev_name.c_str());
        if (0 == if_idx)
        {
            std::stringstream err;
            err << "Could not retrieve if_index for '" << dev_name << "': "
                << std::string(::strerror(errno));
            throw Exception(err.str());
        }

        Link::Ptr ret;
        ret.reset(new Link(GetConnection(), GetLink(if_idx)));
        return ret;
    }


    std::string Manager::GetLink(unsigned int if_idx)
    {
        GVariant* res = Call("GetLink", g_variant_new("(i)", if_idx));
        GLibUtils::checkParams("GetLink", res, "(o)", 1);
        std::string link_path = GLibUtils::ExtractValue<std::string>(res, 0);
        g_variant_unref(res);
        return link_path;
    }

} // namespace resolved
} // namespace DNS
} // namespace NetCfg
