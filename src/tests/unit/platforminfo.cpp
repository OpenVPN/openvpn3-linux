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
    catch (const DBusException& e)
    {
        GTEST_SKIP() << std::string("Could not connect to D-Bus ## ")
                        + std::string(e.what());
    }

    PlatformInfo plinfo(dbc.GetConnection());
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
