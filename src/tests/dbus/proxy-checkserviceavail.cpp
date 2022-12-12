//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
