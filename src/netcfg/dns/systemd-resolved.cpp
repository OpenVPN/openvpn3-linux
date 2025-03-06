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

        upd->dnssec = settings->GetDNSSEC();
        upd->transport = settings->GetDNSTransport();

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
            std::vector<std::string> applied_servers;
            std::vector<std::string> applied_search;

            if (!upd->enable)
            {
                // NetCfgChangeEvents for DNS_SERVER_REMOVED and
                // DNS_SEARCH_REMOVED are sent by the caller of this method
                upd->link->Revert();
            }
            else
            {
                //
                //  Commit the requested DNS resolver setup changes
                //
                signal->LogVerb2("systemd-resolved: [" + upd->link->GetPath()
                                 + "] Committing DNS servers");
                applied_servers = upd->link->SetDNSServers(upd->resolvers);
                signal->LogVerb2("systemd-resolved: [" + upd->link->GetPath()
                                 + "] Committing DNS search domains");
                applied_search = upd->link->SetDomains(upd->search);

                if (feat_dns_default_route
                    && !upd->link->SetDefaultRoute(upd->default_routing))
                {
                    signal->LogWarn("systemd-resolved: Service does not "
                                    "support setting default route for DNS "
                                    "requests. Disabling calling this feature.");
                    feat_dns_default_route = false;
                };

                if (upd->dnssec != openvpn::DnsServer::Security::Unset)
                {
                    configure_dnssec(upd->dnssec, upd->link, signal);
                }

                if (upd->transport != openvpn::DnsServer::Transport::Unset)
                {
                    configure_transport(upd->transport, upd->link, signal);
                }
            }

            resolved::Error::Message::List errors = upd->link->GetErrors();
            bool error_servers = false;
            bool error_domains = false;
            if (!errors.empty())
            {
                for (const auto &err : errors)
                {
                    signal->LogCritical("systemd-resolved: " + err.str());
                    if ("SetDNS" == err.method)
                    {
                        error_servers = true;
                    }
                    if ("SetDomains" == err.method)
                    {
                        error_domains = true;
                    }
                }
                upd->disabled = true;
            }

            //
            // Send the NetworkChange signals
            //
            if (!error_servers)
            {
                for (const auto &srv : applied_servers)
                {
                    NetCfgChangeEvent ev(NetCfgChangeType::DNS_SERVER_ADDED,
                                         upd->link->GetDeviceName(),
                                         {{"dns_server", srv}});
                    signal->NetworkChange(ev);
                }
            }

            if (!error_domains)
            {
                for (const auto &domain : applied_search)
                {
                    NetCfgChangeEvent ev(NetCfgChangeType::DNS_SEARCH_ADDED,
                                         upd->link->GetDeviceName(),
                                         {{"search_domain", domain}});
                    signal->NetworkChange(ev);
                }
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


void SystemdResolved::configure_dnssec(const openvpn::DnsServer::Security &dnssec_mode,
                                       resolved::Link::Ptr link,
                                       NetCfgSignals::Ptr signals)
{
    std::string mode;
    switch (dnssec_mode)
    {
    case openvpn::DnsServer::Security::Yes:
        mode = "yes";
        break;

    case openvpn::DnsServer::Security::No:
        mode = "no";
        break;

    case openvpn::DnsServer::Security::Optional:
        mode = "allow-downgrade";
        break;

    default:
        break;
    }

    if (!mode.empty())
    {
        link->SetDNSSEC(mode);
        signals->LogVerb2("systemd-resolved: ["
                          + link->GetPath()
                          + "] DNSSEC mode set to " + mode);
    }
}


void SystemdResolved::configure_transport(const openvpn::DnsServer::Transport &dns_transport,
                                          resolved::Link::Ptr link,
                                          NetCfgSignals::Ptr signals)

{
    std::string transport;
    switch (dns_transport)
    {
    case openvpn::DnsServer::Transport::HTTPS:
        signals->LogWarn("systemd-resolved: ["
                         + link->GetPath()
                         + "] DNS-over-HTTP is not supported");
        break;
    case openvpn::DnsServer::Transport::Plain:
        transport = "no";
        break;

    case openvpn::DnsServer::Transport::TLS:
        transport = (feat_dnsovertls_enforce
                         ? "yes"
                         : "opportunistic");
        break;

    default:
        break;
    }

    if (!transport.empty())
    {
        bool done = false;
        while (!done)
        {
            try
            {
                link->SetDNSOverTLS(transport);
                signals->LogVerb2("systemd-resolved: "
                                  "Set DNSOverTLS to '"
                                  + transport + "'");
                done = true;
            }
            catch (const resolved::Exception &excp)
            {
                std::string err(excp.what());
                if (err.find("Invalid DNSOverTLS setting:") != std::string::npos)
                {
                    if ("yes" != transport)
                    {
                        done = true;
                        signals->LogError(
                            "systemd-resolved: Failed to set DNSOverTLS"
                            " to '"
                            + transport + "'");
                    }
                    else
                    {
                        // if enforced DoT is failing, try
                        // opportunistic mode instead
                        transport = "opportunistic";
                        feat_dnsovertls_enforce = false;
                        signals->LogInfo(
                            "systemd-resolved: Enforced DNS-over-TLS "
                            "not supported, switching to opportunistic mode");
                    }
                }
                else
                {
                    done = true;
                    signals->LogError(
                        "systemd-resolved: Failed to set DNS transport: "
                        + std::string(excp.what()));
                }
            }
        }
    }
}
