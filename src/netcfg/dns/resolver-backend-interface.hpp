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

#include "netcfg/dns/resolver-settings.hpp"

using namespace openvpn;

namespace NetCfg
{
namespace DNS
{
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
         *  Register DNS resolver settings for a particular VPN session
         *
         * @param settings  ResolverSettings::Ptr containing a pointer to the
         *                  settings to apply
         */
        virtual void Apply(const ResolverSettings::Ptr settings) = 0;


        /**
         *  Apply all resolver settings registered via the Apply() call
         */
        virtual void Commit() = 0;
    };
}
}
