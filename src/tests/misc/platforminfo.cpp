//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
