//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file   src/tests/unit/platforminfo.cpp
 *
 *  @brief  Unit test for the PlatformInfo API
 */

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <gdbuspp/connection.hpp>

#include "common/platforminfo.hpp"


TEST(PlatformInfo, DBus)
{
    auto dbc = DBus::Connection::Create(DBus::BusType::SYSTEM);
    PlatformInfo plinfo(dbc);

    if (!plinfo.DBusAvailable())
    {
        GTEST_SKIP() << "A required D-Bus service isn't available";
    }
    std::string s{plinfo.str()};
    ASSERT_TRUE(s.find("generic:") == std::string::npos)
        << "PlatformInfo D-Bus call failed";
}


TEST(PlatformInfo, uname)
{
    PlatformInfo plinfo(nullptr);

    std::stringstream s;
    s << plinfo;
    ASSERT_TRUE(s.str().find("generic:") == 0)
        << "PlatformInfo uname() failed";
}
