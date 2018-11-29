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
#include <string>

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

    /**
     * Function that binds the the fd to the device of the best route to remote
     * @param fd Socket to bind
     * @param remote remote host to lookup
     * @param ipv6 is remote ipv6
     */
    void protect_socket_binddev(int fd, const std::string & remote, bool ipv6);

    /**
     * Function that set the so_mark for socket
     * @param fd Socket for the SO_MARK to set
     * @param remote remote host to connect to (for logging only)
     * @param somark the value to set SO_MARK to
     */
    void protect_socket_somark(int fd, const std::string & remote, int somark);


    // Workaround to avoid circular dependencies
    CoreTunbuilder* getCoreBuilderInstance();
}
