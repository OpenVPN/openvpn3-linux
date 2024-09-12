//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
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
#include <gio/gio.h>
#include <gdbuspp/glib2/utils.hpp>

#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/netcfg-exception.hpp"

namespace NetCfg {
namespace DNS {

//
// NetCfg::DNS::ResolverSettings
//

ResolverSettings::ResolverSettings(const ssize_t idx)
    : index(idx)
{
}


ResolverSettings::ResolverSettings(const ResolverSettings::Ptr &orig)
    : index(orig->index), enabled(orig->enabled),
      name_servers(orig->name_servers), search_domains(orig->name_servers)
{
}


ResolverSettings::~ResolverSettings() = default;



const ssize_t ResolverSettings::GetIndex() const noexcept
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


void ResolverSettings::SetDeviceName(const std::string &devname) noexcept
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

const char *ResolverSettings::GetDNSScopeStr() const noexcept
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


void ResolverSettings::AddNameServer(const std::string &server)
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


void ResolverSettings::AddSearchDomain(const std::string &domain)
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



void ResolverSettings::SetDNSSEC(const openvpn::DnsServer::Security &mode)
{
    switch (mode)
    {
    case openvpn::DnsServer::Security::No:
    case openvpn::DnsServer::Security::Yes:
    case openvpn::DnsServer::Security::Optional:
    case openvpn::DnsServer::Security::Unset:
        dnssec_mode = mode;
        break;
    default:
        throw NetCfgException("[SetDNSSEC] Invalid DNSSEC value");
    }
}


openvpn::DnsServer::Security ResolverSettings::GetDNSSEC() const
{
    switch (dnssec_mode)
    {
    case openvpn::DnsServer::Security::No:
    case openvpn::DnsServer::Security::Yes:
    case openvpn::DnsServer::Security::Optional:
    case openvpn::DnsServer::Security::Unset:
        return dnssec_mode;
    default:
        throw NetCfgException("[GetDNSSEC] Invalid DNSSEC value");
    }
}


std::string ResolverSettings::GetDNSSEC_string() const
{
    switch (dnssec_mode)
    {
    case openvpn::DnsServer::Security::No:
        return "no";
    case openvpn::DnsServer::Security::Yes:
        return "yes";
    case openvpn::DnsServer::Security::Optional:
        return "optional";
    case openvpn::DnsServer::Security::Unset:
        return "unset";
    default:
        throw NetCfgException("[GetDNSSEC_string] Invalid DNSSEC value");
    }
}



const std::string ResolverSettings::SetDNSScope(GVariant *params)
{
    std::string params_type = glib2::DataType::Extract(params);
    if ("s" != params_type)
    {
        throw NetCfgException("[SetDNSScope] Invalid D-Bus data type: " + params_type);
    }

    std::string recv_scope = glib2::Value::Get<std::string>(params);
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
    std::string params_type = glib2::DataType::Extract(params);
    if ("(as)" != params_type)
    {
        throw NetCfgException("[AddNameServers] Invalid D-Bus data type");
    }

    std::string ret{};
    g_variant_ref_sink(params);
    for (const auto &srv : glib2::Value::ExtractVector<std::string>(params))
    {
        auto needle = std::find(name_servers.begin(),
                                name_servers.end(),
                                srv);
        if (name_servers.end() == needle)
        {
            name_servers.push_back(srv);
        }
        ret += (!ret.empty() ? ", " : "") + srv;
    }
    return ret;
}


void ResolverSettings::AddSearchDomains(GVariant *params)
{
    std::string params_type = glib2::DataType::Extract(params);
    if ("(as)" != params_type)
    {
        throw NetCfgException("[AddSearchDomain] Invalid D-Bus data type");
    }

    g_variant_ref_sink(params);
    for (const auto &dom : glib2::Value::ExtractVector<std::string>(params))
    {
        auto needle = std::find(search_domains.begin(),
                                search_domains.end(),
                                dom);
        if (search_domains.end() == needle)
        {
            search_domains.push_back(dom);
        }
    }
}


openvpn::DnsServer::Security ResolverSettings::SetDNSSEC(GVariant *params)
{
    std::string params_type = glib2::DataType::Extract(params);
    if ("(s)" != params_type)
    {
        throw NetCfgException("[SetDNSSEC] Invalid D-Bus data type");
    }

    auto mode = glib2::Value::Extract<std::string>(params, 0);
    if ("yes" == mode)
    {
        dnssec_mode = openvpn::DnsServer::Security::Yes;
    }
    else if ("no" == mode)
    {
        dnssec_mode = openvpn::DnsServer::Security::No;
    }
    else if ("optional" == mode)
    {
        dnssec_mode = openvpn::DnsServer::Security::Optional;
    }
    else
    {
        throw NetCfgException("[SetDNSSEC] Invalid DNSSEC mode: '" + mode + "'");
    }
    return dnssec_mode;
}

} // namespace DNS
} // namespace NetCfg
