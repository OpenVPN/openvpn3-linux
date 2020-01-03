//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019 - 2020  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   resolver-settings.cpp
 *
 *
 * @brief  Implements the class which is responsible for managing DNS
 *         resolver settings
 */

#include <string>
#include <vector>

#include "dbus/core.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/netcfg-exception.hpp"

namespace NetCfg
{
namespace DNS
{
    //
    // NetCfg::DNS::ResolverSettings
    //

    ResolverSettings::ResolverSettings(ssize_t idx) : index(idx)
    {
    }

    ResolverSettings::~ResolverSettings() = default;


    ssize_t ResolverSettings::GetIndex() const noexcept
    {
        return index;
    }


    void ResolverSettings::Enable() noexcept
    {
        enabled = true;
    }


    void ResolverSettings::Disable() noexcept
    {
        enabled = false;
    }


    bool ResolverSettings::GetEnabled() const noexcept
    {
        return enabled;
    }


    void ResolverSettings::AddNameServer(const std::string& server)
    {
        name_servers.push_back(server);
    }

    void ResolverSettings::ClearNameServers()
    {
        name_servers.clear();
    }

    std::vector<std::string> ResolverSettings::GetNameServers() const noexcept
    {
        return name_servers;
    }

    void ResolverSettings::AddSearchDomain(const std::string& domain)
    {
        search_domains.push_back(domain);
    }

    void ResolverSettings::ClearSearchDomains()
    {
        search_domains.clear();
    }

    std::vector<std::string> ResolverSettings::GetSearchDomains() const noexcept
    {
        return search_domains;
    }


    const std::string ResolverSettings::AddNameServers(GVariant *params)
    {
        std::string params_type(g_variant_get_type_string(params));
        if ("(as)" != params_type)
        {
            throw NetCfgException("[AddResolveServers] Invalid D-Bus data type");
        }

        GVariantIter *srvlist = nullptr;
        g_variant_get(params, "(as)", &srvlist);
        if (nullptr == srvlist)
        {
            throw NetCfgException("[AddResolveServers] Failed to extract parameters");
        }

        GVariant *srv = nullptr;
        bool first = true;
        std::string ret;
        while ((srv = g_variant_iter_next_value(srvlist)))
        {
            gsize len;
            std::string v(g_variant_get_string(srv, &len));
            name_servers.push_back(v);
            ret += (!first ? std::string(", ") : std::string("")) + v;
            g_variant_unref(srv);
        }
        g_variant_iter_free(srvlist);

        return ret;
    }


    void ResolverSettings::AddSearchDomains(GVariant *params)
    {
        std::string params_type(g_variant_get_type_string(params));
        if ("(as)" != params_type)
        {
            throw NetCfgException("[AddSearchDomain] Invalid D-Bus data type");
        }

        GVariantIter *srchlist = nullptr;
        g_variant_get(params, "(as)", &srchlist);
        if (nullptr == srchlist)
        {
            throw NetCfgException("[AddSearchDomain] Failed to extract parameters");
        }

        GVariant *srchdom = nullptr;;
        while ((srchdom = g_variant_iter_next_value(srchlist)))
        {
            gsize len;
            std::string v(g_variant_get_string(srchdom, &len));
            search_domains.push_back(v);
            g_variant_unref(srchdom);
        }
        g_variant_iter_free(srchlist);
    }
} // namespace DNS
} // namespace NetCfg
