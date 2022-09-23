//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   src/tests/misc/platforminfo.cpp
 *
 * @brief  Tests the PlatformInfo API impementation
 */

#include <iostream>
#include <ostream>
#include "dbus/core.hpp"
#include "common/platforminfo.hpp"


int main()
{
    {
        DBus dbc(G_BUS_TYPE_SYSTEM);
        dbc.Connect();

        PlatformInfo plinfo(dbc.GetConnection());
        std::cout << "D-Bus approach: " << plinfo << std::endl;
    }

    {
        PlatformInfo plinfo(nullptr);
        std::cout << "Incorrect D-Bus setup: " << plinfo << std::endl;
    }
    return 0;
}