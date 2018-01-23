//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   getconfig-acl.cpp
 *
 * @brief  Simple test of the GetOwner() and GetAccessList() methods in
 *         the OpenVPN3ConfigurationProxy
 */

#include <iostream>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "ovpn3cli/lookup.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <config obj path>"
                  << std::endl;
        return 1;
    }

    OpenVPN3ConfigurationProxy config(G_BUS_TYPE_SYSTEM, argv[1]);

    std::cout << "Configuration owner: (" << config.GetOwner()
              << ") " << lookup_username(config.GetOwner())
              << std::endl;
    for (uid_t uid : config.GetAccessList())
    {
        std::cout << "uid: " << uid
                  << "  user: " << lookup_username(uid)
                  << std::endl;
    }
    return 0;
}
