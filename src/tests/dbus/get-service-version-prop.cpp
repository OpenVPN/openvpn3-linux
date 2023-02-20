//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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
    catch (std::exception &err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
