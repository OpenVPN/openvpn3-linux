//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   settings-manager.cpp
 *
 *
 * @brief  Main manager object for all DNS resolver settings (implementation)
 */

#include "dbus/core.hpp"
#include "netcfg/netcfg-exception.hpp"
#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/settings-manager.hpp"

namespace NetCfg {
namespace DNS {

//
// NetCfg::DNS::SettingsManager
//

SettingsManager::SettingsManager(ResolverBackendInterface::Ptr be)
    : backend(be)
{
}


SettingsManager::~SettingsManager() = default;


const std::string SettingsManager::GetBackendInfo() const
{
    return backend->GetBackendInfo();
}


const ApplySettingsMode SettingsManager::GetApplyMode() const
{
    return backend->GetApplyMode();
}


ResolverSettings::Ptr SettingsManager::NewResolverSettings()
{
    ResolverSettings::Ptr settings;
    settings.reset(new ResolverSettings(++resolver_idx));
    resolvers[resolver_idx] = settings;
    return settings;
}


void SettingsManager::ApplySettings(NetCfgSignals *signal)
{
    // The list of ResolverSettings need to be applied in the reverse order.
    // This ensures the last connected VPN server has precedence.
    for (auto rslv = resolvers.rbegin(); rslv != resolvers.rend(); rslv++)
    {
        if (rslv->second->ChangesAvailable() && !rslv->second->GetRemovable())
        {
            backend->Apply(rslv->second);
        }
    }
    backend->Commit(signal);

    std::vector<NetCfgChangeEvent> notif; // NetworkChange notification queue
    std::vector<ssize_t> remove_list;
    for (const auto &rslv : resolvers)
    {
        ResolverSettings::Ptr settings = rslv.second;
        if (!settings->GetRemovable())
        {
            break;
        }

        ssize_t idx = settings->GetIndex();
        if (idx >= 0)
        {
            // If we have a NetCfgSignal object available, use that to
            // send NetworkChange events with the removed DNS resolver settings
            if (signal)
            {
                std::vector<std::string> name_servers = settings->GetNameServers(true);
                if (name_servers.size() > 0)
                {
                    for (const auto &s : name_servers)
                    {
                        notif.push_back(NetCfgChangeEvent(NetCfgChangeType::DNS_SERVER_REMOVED,
                                                          "",
                                                          {{"dns_server", s}}));
                    }
                }

                std::vector<std::string> search_domains = settings->GetSearchDomains(true);
                if (search_domains.size() > 0)
                {
                    for (const auto &s : search_domains)
                    {
                        notif.push_back(NetCfgChangeEvent(NetCfgChangeType::DNS_SEARCH_REMOVED,
                                                          "",
                                                          {{"search_domain", s}}));
                    }
                }

                // Send all NetworkChange events in the notification queue
                for (const auto &ev : notif)
                {
                    signal->NetworkChange(ev);
                }
            }
            remove_list.push_back(idx);
        }
    }

    for (const auto idx : remove_list)
    {
        resolvers.erase(idx);
    }
}


std::vector<std::string> SettingsManager::GetDNSservers() const
{
    std::vector<std::string> ret;
    for (const auto &rslv : resolvers)
    {
        std::vector<std::string> s = rslv.second->GetNameServers();
        ret.insert(ret.end(), s.begin(), s.end());
    }
    return ret;
}


std::vector<std::string> SettingsManager::GetSearchDomains() const
{
    std::vector<std::string> ret;
    for (const auto &rslv : resolvers)
    {
        std::vector<std::string> s = rslv.second->GetSearchDomains();
        ret.insert(ret.end(), s.begin(), s.end());
    }
    return ret;
}


} // namespace DNS
} // namespace NetCfg
