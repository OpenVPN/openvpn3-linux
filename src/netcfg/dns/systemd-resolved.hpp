//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <openvpn/common/rc.hpp>

using namespace openvpn;

#include "dbus/core.hpp"

#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"

using namespace NetCfg::DNS;

namespace NetCfg {
namespace DNS {

class SystemdResolved : public ResolverBackendInterface,
                        public resolved::Manager
{
  public:
    typedef RCPtr<SystemdResolved> Ptr;

    /**
     *  Internal structure used to queue DNS configuration updates.
     *
     *  The @Apply() method will extract all the needed information
     *  and save it for the @Commit() method to be called.
     */
    struct updateQueueEntry
    {
        bool enable;
        bool disabled = false;
        resolved::Link::Ptr link;
        resolved::ResolverRecord::List resolver;
        resolved::SearchDomain::List search;
    };


    /**
     *  The SystemdResolved class bridges the information passed from
     *  the VPN session through the ResolverBackendInterface and prepares
     *  it to be consumed by the systemd-resolved process.
     *  (org.freedesktop.resolve1 D-Bus service).
     *
     * @param dbc   GDBusConnection pointer to a valid D-Bus connection to
     *              use.
     */
    SystemdResolved(GDBusConnection *dbc);
    ~SystemdResolved();


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
    void Commit(NetCfgSignals *signal) override;


  private:
    std::vector<updateQueueEntry> update_queue;
};
} // namespace DNS
} // namespace NetCfg
