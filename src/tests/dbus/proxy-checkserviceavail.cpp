//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN Inc <sales@openvpn.net>
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
 * @file   proxy-checkserviceavail.cpp
 *
 * @brief  [enter description of what this file contains]
 */


#include <iostream>

#include "dbus/core.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
    if (3 != argc)
    {
        std::cerr << "Usage: " << argv[0] << " <D-Bus service name> "
                  << "<D-Bus interface>" << std::endl;
        return 2;
    }

    DBusProxy prx(G_BUS_TYPE_SYSTEM, argv[1], argv[2], "/");
    prx.CheckServiceAvail();
    std::cout << "Service is available" << std::endl;

    return 0;
}
