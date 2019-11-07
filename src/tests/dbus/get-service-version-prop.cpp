//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   get-service-version-prop.cpp
 *
 * @brief  Simple test program to test looking up the 'version'
 *         property in a D-Bus service
 */

#include <iostream>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0]
                  << " <service name> <service interface> <service d-bus path>" << std::endl;
        return 1;
    }

    try
    {
        std::string service(argv[1]);
        std::string interface(argv[2]);
        std::string path(argv[3]);
        DBusProxy prx(G_BUS_TYPE_SYSTEM, service, interface, path);

        std::cout << "Service version: "
                  << prx.GetServiceVersion()
                  << std::endl;
        return 0;
    }
    catch (std::exception& err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
