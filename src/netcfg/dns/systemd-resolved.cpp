//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved.cpp
 *
 * @brief  This implements systemd-resolved support for DNS resolver settings
 */


#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include <openvpn/common/rc.hpp>
#include <openvpn/addr/ip.hpp>

#include "common/timestamp.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"
#include "netcfg/dns/systemd-resolved.hpp"
#include "netcfg/netcfg-exception.hpp"

using namespace NetCfg::DNS;
using namespace NetCfg::DNS::resolved;

SystemdResolved::SystemdResolved(GDBusConnection *dbc)
    : resolved::Manager(dbc)
{
}


SystemdResolved::~SystemdResolved() = default;

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
    SystemdResolved::updateQueueEntry upd;
    upd.link = RetrieveLink(settings->GetDeviceName());
    upd.enable = settings->GetEnabled();

    if (upd.enable)
    {

        for (const auto &r : settings->GetNameServers())
        {
            if (!IP::Addr::is_valid(r))
            {
                // Should log invalid IP addresses
                continue;
            }
            IP::Addr addr(r);
            upd.resolver.push_back(ResolverRecord((addr.is_ipv6() ? AF_INET6 : AF_INET),
                                                  addr.to_string()));
        }

        for (const auto &sd : settings->GetSearchDomains())
        {
            upd.search.push_back(SearchDomain(sd, false));
        }

        if (settings->GetDNSScope() == DNS::Scope::GLOBAL)
        {
            upd.search.push_back(SearchDomain(".", true));
        }
    }
    update_queue.push_back(upd);
}


void SystemdResolved::Commit(NetCfgSignals *signal)
{
    for (auto &upd : update_queue)
    {
        if (upd.disabled)
        {
            continue;
        }
        try
        {
            if (upd.enable)
            {
                signal->LogVerb2("systemd-resolved: [" + upd.link->GetPath()
                                 + "] Committing DNS servers");
                upd.link->SetDNSServers(upd.resolver);
                signal->LogVerb2("systemd-resolved: [" + upd.link->GetPath()
                                 + "] Committing DNS search domains");
                upd.link->SetDomains(upd.search);
            }
            else
            {
                upd.link->Revert();
            }
        }
        catch (const DBusProxyAccessDeniedException &excp)
        {
            signal->LogCritical("systemd-resolved: " + std::string(excp.what()));
            upd.disabled = true;
        }
        catch (const DBusException &excp)
        {
            signal->LogCritical("systemd-resolved: " + std::string(excp.what()));
            upd.disabled = true;
        }
        catch (const std::exception &excp)
        {
            signal->LogError("systemd-resolved: " + std::string(excp.what()));
            upd.disabled = true;
        }
    }
    update_queue.clear();
}
