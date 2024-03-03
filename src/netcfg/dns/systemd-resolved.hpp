//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved.hpp
 *
 * @brief  Implements systemd-resolved support for configuring DNS resolver
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"

using namespace NetCfg::DNS;


namespace NetCfg {
namespace DNS {


/**
 *  Integration for configuring DNS resolver settings using systemd-resolved
 */
class SystemdResolved : public ResolverBackendInterface
{
  public:
    using Ptr = std::shared_ptr<SystemdResolved>;

    /**
     *  Internal structure used to queue DNS configuration updates.
     *
     *  The @Apply() method will extract all the needed information
     *  and save it for the @Commit() method to be called.
     */
    struct updateQueueEntry
    {
      public:
        using Ptr = std::shared_ptr<updateQueueEntry>;

        [[nodiscard]] static Ptr Create(resolved::Link::Ptr link,
                                        const bool enabled)
        {
            return Ptr(new updateQueueEntry(link, enabled));
        }

        bool enable = false;                       ///< enabled by config profile
        bool disabled = false;                     ///< disabled internally by errors
        bool default_routing = false;              ///< set the default routing dns flag
        resolved::Link::Ptr link = nullptr;        ///< Pointer to the resolved::Link interface object
        resolved::ResolverRecord::List resolver{}; ///< List of DNS resolver IP addresses for this link
        resolved::SearchDomain::List search{};     ///< List of DNS search domains to add for this link


      private:
        updateQueueEntry(resolved::Link::Ptr l, const bool enabl)
            : enable(enabl), link(l)
            {}
    };


    /**
     *  The SystemdResolved class bridges the information passed from
     *  the VPN session through the ResolverBackendInterface and prepares
     *  it to be consumed by the systemd-resolved process.
     *  (org.freedesktop.resolve1 D-Bus service).
     *
     * @param conn   DBus::Connection::Ptr to the bus used to communicate with
     *               org.freedesktop.resolve1 (aka systemd-resolved)
     *
     * @return SystemdResolved::Ptr to the main object providing the
     *         systemd-resolved communication channel
     */
    [[nodiscard]] static Ptr Create(DBus::Connection::Ptr conn)
    {
        return Ptr(new SystemdResolved(conn));
    }
    ~SystemdResolved() noexcept = default;


    /**
     *  Retrieve information about the configured resolver backend.
     *  Used for log events
     *
     * @return Returns a constant std::string with backend details.
     */
    const std::string GetBackendInfo() const noexcept override;


    /**
     *  Retrieve when the backend can apply the DNS resolver settings.
     *  Normally it is applied before the tun interface configuration,
     *  but some backends may need information about the device to
     *  complete the configuration.
     *
     *  For the SystemdResolved implementation, this will always return
     *  MODE_POST as it needs information about the tun interface to
     *  properly configure split-dns.
     *
     * @returns NetCfg::DNS:ApplySettingsMode
     */
    const ApplySettingsMode GetApplyMode() const noexcept override;


    /**
     *  Add new DNS resolver settings.  This may be called multiple times
     *  in the order it will be processed by the resolver backend.
     *
     * @param settings  A ResolverSettings::Ptr object containing the
     *                  DNS resolver configuraiton data
     */
    void Apply(const ResolverSettings::Ptr settings) override;


    /**
     *  Completes the DNS resolver configuration by performing the
     *  changes on the system.
     *
     *  @param signal  Pointer to a NetCfgSignals object where
     *                 "NetworkChange" signals will be issued
     */
    void Commit(NetCfgSignals::Ptr signal) override;


  private:
    std::vector<updateQueueEntry::Ptr> update_queue = {};
    resolved::Manager::Ptr sdresolver = nullptr;
    bool feat_dns_default_route = true;

    SystemdResolved(DBus::Connection::Ptr dbc);
};
} // namespace DNS
} // namespace NetCfg
