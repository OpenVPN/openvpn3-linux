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
 * @file   dns-resolver-manager.hpp
 *
 * @brief  Main manager object for all DNS resolver settings (declaration)
 */

#pragma once

#include <map>
#include <vector>

#include <openvpn/common/rc.hpp>

#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-settings.hpp"
#include "netcfg/dns/resolver-backend-interface.hpp"

using namespace openvpn;
using namespace NetCfg::DNS;

namespace NetCfg
{
namespace DNS
{
    /**
     *  Manager object for all DNS settings on the system
     *
     *  This depends on a backend handler object (ResolverBackendInterface)
     *  which will perform the changes on the system.
     */
    class SettingsManager: public virtual RC<thread_unsafe_refcount>
        {
        public:
            typedef RCPtr<SettingsManager> Ptr;

            SettingsManager(ResolverBackendInterface::Ptr be);
            ~SettingsManager();

            /**
             *  Retrieve information about the configured DNS Settings backend
             *
             * @return  Returns a std::string containing the details
             */
            const std::string GetBackendInfo() const;

            const ApplySettingsMode GetApplyMode() const;

            /**
             *  Allocate a new set of DNS settings for the DNS resolver.
             *
             *  This operation is transaction oriented and will not be applied
             *  until the ApplySettings() method has been called.   The order
             *  of execution will be the reverse order of the
             *  NewResolverSettings() calls.  The latest call gets precedence
             *  over the other calls.
             *
             * @return  Returns a ResolverSettings::Ptr to an object which
             *          need to be configured with the DNS settings for the
             *          VPN session it is associated with
             */
            ResolverSettings::Ptr NewResolverSettings();


            /**
             *   Apply all configured DNS settings
             */
            void ApplySettings(NetCfgSignals *signal);

            /**
             *  Retrieve the full list of all configured DNS servers
             *  for all VPN sessions
             *
             * @return  Returns a std::vector<std::string> with all DNS
             *          servers.  They are ordered according to the reverse
             *          order the VPN sessions were started.  The last entries
             *          will be the system configured DNS servers.
             */
            std::vector<std::string> GetDNSservers() const;

            /**
             *  Retrieve the full list of all search domains
             *
             * @return  Returns a std::vector<std::string> with all DNS
             *          search domains for both VPN sessions and the
             *          configured system search domains.
             */
            std::vector<std::string> GetSearchDomains() const;


    private:
        ResolverBackendInterface::Ptr backend;
        ssize_t resolver_idx = -1;
        std::map<size_t, ResolverSettings::Ptr> resolvers{};
    };

} // namespace DNS
} // namespace NetCfg
