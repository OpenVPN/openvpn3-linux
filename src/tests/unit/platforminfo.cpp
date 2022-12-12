//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
//

/**
 *  @file   src/tests/unit/platforminfo.cpp
 *
 *  @brief  Unit test for the PlatformInfo API
 */

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include "dbus/core.hpp"
#include "common/platforminfo.hpp"
#include "dbus/exceptions.hpp"


TEST(PlatformInfo, DBus)
{
    DBus dbc(G_BUS_TYPE_SYSTEM);
    try
    {
        dbc.Connect();
    }
    catch (const DBusException &e)
    {
        GTEST_SKIP() << std::string("Could not connect to D-Bus ## ")
                            + std::string(e.what());
    }

    PlatformInfo plinfo(dbc.GetConnection());

    try
    {
        plinfo.GetStringProperty("OperatingSystemCPEName");
    }
    catch (const DBusException& e)
    {
        const std::string what{e.what()};
        if (what.find("was not provided by any") != std::string::npos)
        {
            GTEST_SKIP() << "A required service isn't available ## " + what;
        }
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
