//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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
 * @file   core-tunbuilder.hpp
 *
 * @brief This class will reuse the OpenVPN3 tun builder to create
 *        and manage a tun device
 *
 *
 */

#pragma once
#include <openvpn/common/rc.hpp>

class NetCfgDevice;

namespace openvpn
{
    class CoreTunbuilder : public RC<thread_safe_refcount>
    {
    public:
        virtual int establish(const NetCfgDevice& netCfgDevice) = 0;
        virtual void teardown(bool disconnect) = 0;
    };

    // Forward declaration to allow friend
    class CoreTunbuilderImpl;

    // Workaround to avoid circular dependencies
    CoreTunbuilder* getCoreBuilderInstance();
}
