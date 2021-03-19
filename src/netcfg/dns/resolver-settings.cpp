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

#include <algorithm>
#include <string>
#include <vector>

#include <glib.h>

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


    void ResolverSettings::PrepareRemoval() noexcept
    {
        prepare_removal = true;
    }


    bool ResolverSettings::GetRemovable() const noexcept
    {
        return prepare_removal;
    }


    bool ResolverSettings::ChangesAvailable() const noexcept
    {
        return (name_servers.size() > 0 || search_domains.size() > 0);
    }


    void ResolverSettings::SetDeviceName(const std::string& devname) noexcept
    {
        device_name = devname;
    }


    std::string ResolverSettings::GetDeviceName() const noexcept
    {
        return device_name;
    }


    void ResolverSettings::SetDNSScope(const DNS::Scope new_scope) noexcept
    {
        scope = new_scope;
    }


    const DNS::Scope ResolverSettings::GetDNSScope() const noexcept
    {
        return scope;
    }

    const char* ResolverSettings::GetDNSScopeStr() const noexcept
    {
        switch (scope)
        {
        case DNS::Scope::GLOBAL:
            return "global";
        case DNS::Scope::TUNNEL:
            return "tunnel";
        }
        return "";
    }


    void ResolverSettings::AddNameServer(const std::string& server)
    {
        // Avoid adding duplicated entries
        auto needle = std::find(name_servers.begin(),
                                name_servers.end(),
                                server);
        if (std::end(name_servers) == needle)
        {
            name_servers.push_back(server);
        }
    }

    void ResolverSettings::ClearNameServers()
    {
        name_servers.clear();
    }

    std::vector<std::string> ResolverSettings::GetNameServers(bool removable) const noexcept
    {
        return (!prepare_removal || removable ? name_servers : std::vector<std::string>{});
    }

    void ResolverSettings::AddSearchDomain(const std::string& domain)
    {
        // Avoid adding duplicated entries
        auto needle = std::find(search_domains.begin(),
                                search_domains.end(),
                                domain);
        if (std::end(search_domains) == needle)
        {
            search_domains.push_back(domain);
        }
    }

    void ResolverSettings::ClearSearchDomains()
    {
        search_domains.clear();
    }

    std::vector<std::string> ResolverSettings::GetSearchDomains(bool removable) const noexcept
    {
        return (!prepare_removal || removable ? search_domains : std::vector<std::string>{});
    }


    const std::string ResolverSettings::SetDNSScope(GVariant *params)
    {
        std::string params_type(g_variant_get_type_string(params));
        if ("s" != params_type)
        {
            throw NetCfgException("[SetDNSScope] Invalid D-Bus data type: " + params_type);
        }

        std::string recv_scope(g_variant_get_string(params, nullptr));
        if ("global" == recv_scope)
        {
            scope = DNS::Scope::GLOBAL;
            return recv_scope;
        }
        else if ("tunnel" == recv_scope)
        {
            scope = DNS::Scope::TUNNEL;
            return recv_scope;
        }
        throw NetCfgException("[SetDNSScope] Invalid DNS Scope value: " + recv_scope);
    }


    const std::string ResolverSettings::AddNameServers(GVariant *params)
    {
        std::string params_type(g_variant_get_type_string(params));
        if ("(as)" != params_type)
        {
            throw NetCfgException("[AddNameServers] Invalid D-Bus data type");
        }

        GVariantIter *srvlist = nullptr;
        g_variant_get(params, "(as)", &srvlist);
        if (nullptr == srvlist)
        {
            throw NetCfgException("[AddNameServers] Failed to extract parameters");
        }

        GVariant *srv = nullptr;
        std::string ret;
        while ((srv = g_variant_iter_next_value(srvlist)))
        {
            gsize len;
            std::string v(g_variant_get_string(srv, &len));

            // Avoid adding duplicated entries
            auto needle = std::find(name_servers.begin(),
                                    name_servers.end(),
                                    v);
            if (std::end(name_servers) == needle)
            {
                name_servers.push_back(v);
            }
            ret += (!ret.empty() ? ", " : "" ) + v;

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

        GVariant *srchdom = nullptr;
        while ((srchdom = g_variant_iter_next_value(srchlist)))
        {
            gsize len;
            std::string v(g_variant_get_string(srchdom, &len));

            // Avoid adding duplicated entries
            auto needle = std::find(search_domains.begin(),
                                    search_domains.end(),
                                    v);
            if (std::end(search_domains) == needle)
            {
                search_domains.push_back(v);
            }

            g_variant_unref(srchdom);
        }
        g_variant_iter_free(srchlist);
    }
} // namespace DNS
} // namespace NetCfg
