//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved.cpp
 *
 * @brief  This implements systemd-resolved support for DNS resolver settings
 */


#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "build-config.h"
#include <openvpn/addr/ip.hpp>

#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"
#include "netcfg/dns/systemd-resolved.hpp"
#include "netcfg/netcfg-exception.hpp"


using namespace NetCfg::DNS;
using namespace NetCfg::DNS::resolved;


SystemdResolved::SystemdResolved(DBus::Connection::Ptr dbc)
{
    sdresolver = resolved::Manager::Create(dbc);
}


const std::string SystemdResolved::GetBackendInfo() const noexcept
{
    return std::string("systemd-resolved DNS configuration backend");
}


const ApplySettingsMode SystemdResolved::GetApplyMode() const noexcept
{
    return ApplySettingsMode::MODE_POST;
}


void SystemdResolved::Apply(const ResolverSettings::Ptr settings)
{
    Link::Ptr link = nullptr;
    try
    {
        link = sdresolver->RetrieveLink(settings->GetDeviceName());
        if (!link)
        {
            return;
        }
    }
    catch (const resolved::Exception &excp)
    {
        std::ostringstream err;
        err << "No link device available in systemd-resolved for "
            << settings->GetDeviceName() << ": " << excp.what();
        throw NetCfgException(err.str());
    }

    auto upd = SystemdResolved::updateQueueEntry::Create(link,
                                                         settings->GetEnabled());
    if (upd->enable)
    {

        for (const auto &r : settings->GetNameServers())
        {
            if (!openvpn::IP::Addr::is_valid(r))
            {
                // Should log invalid IP addresses
                continue;
            }
            openvpn::IP::Addr addr(r);
            upd->resolvers.push_back(resolved::IPAddress(addr.to_string(),
                                                         addr.is_ipv6() ? AF_INET6 : AF_INET));
        }

        for (const auto &sd : settings->GetSearchDomains())
        {
            // Consider if domains should probably be set to routing domain
            // https://systemd.io/RESOLVED-VPNS/ - if in split-dns mode (tunnel scope)
            // TODO: Look into getting routing domains setup for reverse DNS lookups
            //        based on pushed routes
            upd->search.push_back(SearchDomain(sd, false));
        }

        if (settings->GetDNSScope() == DNS::Scope::GLOBAL)
        {
            upd->search.push_back(SearchDomain(".", true));
            upd->default_routing = true;
        }
        else
        {
            upd->default_routing = false;
        }
    }
    update_queue.insert(update_queue.begin(), upd);
}


void SystemdResolved::Commit(NetCfgSignals::Ptr signal)
{

    for (uint32_t i = 0; i < update_queue.size(); ++i)
    {
        SystemdResolved::updateQueueEntry::Ptr upd = update_queue.back();
        update_queue.pop_back();
        if (!upd->link)
        {
            continue;
        }
        if (upd->disabled)
        {
            continue;
        }
        try
        {
            if (upd->enable)
            {
                signal->LogVerb2("systemd-resolved: [" + upd->link->GetPath()
                                 + "] Committing DNS servers");
                auto applied_servers = upd->link->SetDNSServers(upd->resolvers);
                signal->LogVerb2("systemd-resolved: [" + upd->link->GetPath()
                                 + "] Committing DNS search domains");
                auto applied_search = upd->link->SetDomains(upd->search);


                if (feat_dns_default_route
                    && !upd->link->SetDefaultRoute(upd->default_routing))
                {
                    signal->LogWarn("systemd-resolved: Service does not "
                                    "support setting default route for DNS "
                                    "requests. Disabling calling this feature.");
                    feat_dns_default_route = false;
                };

                for (const auto &srv : applied_servers)
                {
                    NetCfgChangeEvent ev(NetCfgChangeType::DNS_SERVER_ADDED,
                                         upd->link->GetDeviceName(),
                                         {{"dns_server", srv}});
                    signal->NetworkChange(ev);
                }

                for (const auto &domain : applied_search)
                {
                    NetCfgChangeEvent ev(NetCfgChangeType::DNS_SEARCH_ADDED,
                                         upd->link->GetDeviceName(),
                                         {{"search_domain", domain}});
                    signal->NetworkChange(ev);
                }
            }
            else
            {
                // NetCfgChangeEvents for DNS_SERVER_REMOVED and
                // DNS_SEARCH_REMOVED are sent by the caller of this method
                upd->link->Revert();
            }
        }
        catch (const DBus::Exception &excp)
        {
            signal->LogCritical("systemd-resolved: " + std::string(excp.what()));
            upd->disabled = true;
        }
        catch (const std::exception &excp)
        {
            signal->LogError("systemd-resolved: " + std::string(excp.what()));
            upd->disabled = true;
        }
        upd.reset();
    }
    update_queue.clear();
}
