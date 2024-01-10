//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   src/tests/misc/platforminfo.cpp
 *
 * @brief  Tests the PlatformInfo API impementation
 */

#include <iostream>
#include <ostream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>

#include "common/platforminfo.hpp"


int main()
{
    {
        auto dbc = DBus::Connection::Create(DBus::BusType::SYSTEM);

        PlatformInfo plinfo(dbc);
        std::cout << "D-Bus approach: " << plinfo << std::endl;
    }

    {
        PlatformInfo plinfo(nullptr);
        std::cout << "Incorrect D-Bus setup: " << plinfo << std::endl;
    }
    return 0;
}
