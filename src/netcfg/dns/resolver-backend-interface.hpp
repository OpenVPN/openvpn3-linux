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
 * @file   resolver-backend-interface.hpp
 *
 *
 * @brief  Definition of an abstract interface the SettingsManager expects
 *         from the resolver backend which performs changes on the system
 */
#pragma once

#include <openvpn/common/rc.hpp>

#include "netcfg/netcfg-signals.hpp"
#include "netcfg/dns/resolver-settings.hpp"

using namespace openvpn;

namespace NetCfg
{
namespace DNS
{
    /**
     *  Modes of operations when applying DNS resolver settings.  Depending
     *  on the the backend, they can be applied before or after the tun
     *  interface has been configured.
     */
    enum class ApplySettingsMode
    {
        MODE_PRE,  ///<  Settings are applied before device config
        MODE_POST  ///<  Settings are applied after device config
    };


    /**
      *  Definition of the basic API for resolver settings implementations.
      *
      *  This implementation needs to account for one such object per
      *  running VPN session.
      *
      */
    class ResolverBackendInterface : public virtual RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<ResolverBackendInterface> Ptr;

        ResolverBackendInterface() = default;
        virtual ~ResolverBackendInterface() = default;

        /**
         *  Provide some information for logging about the configured
         *  resolver backend.
         *
         * @return  Returns a std::string with the information.
         */
        virtual const std::string GetBackendInfo() const noexcept = 0;


        /**
         *  Retrieve when the backend can apply the DNS resolver settings.
         *  Normally it is applied before the tun interface configuration,
         *  but some backends may need information about the device to
         *  complete the configuration.
         *
         * @returns NetCfg::DNS:ApplySettingsMode
         */
        virtual const ApplySettingsMode GetApplyMode() const noexcept = 0;


        /**
         *  Register DNS resolver settings for a particular VPN session
         *
         * @param settings  ResolverSettings::Ptr containing a pointer to the
         *                  settings to apply
         */
        virtual void Apply(const ResolverSettings::Ptr settings) = 0;


        /**
         *  Apply all resolver settings registered via the Apply() call
         *
         *  @param signals   Pointer to a NetCfgSignals object which will
         *                   send "NetworkChange" notifications for DNS
         *                   resolver changes
         */
        virtual void Commit(NetCfgSignals *signals) = 0;
    };
}
}
