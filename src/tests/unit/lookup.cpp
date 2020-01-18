//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   lookup.cpp
 *
 * @brief  Simple unit tests which checks lookup_uid() and lookup_username()
 *
 */

#include <iostream>
#include <gtest/gtest.h>

#include "common/utils.hpp"
#include "common/lookup.hpp"

namespace unittest
{

TEST(lookup, root_username)
{
    uid_t root = lookup_uid("root");
    ASSERT_EQ(root, 0);
}

TEST(lookup, nobody_username)
{
    uid_t nobody;
    try
    {
        nobody = lookup_uid("nobody");
    }
    catch (const LookupException&)
    {
        GTEST_SKIP() << "User nobody does not exist on this system";
    }
    ASSERT_NE(nobody, 0);
}

TEST(lookup, nonexisting_username)
{
    EXPECT_THROW(lookup_uid("nonexiting_user"), LookupException);
}

TEST(lookup, uid_0)
{
    std::string root_username = lookup_username(0);
    ASSERT_EQ(root_username, "root");
}

TEST(lookup, root_groupname)
{
    gid_t root_gid = lookup_gid("root");
    ASSERT_EQ(root_gid, 0);
}


TEST(lookup, nonexisting_groupname)
{
    EXPECT_THROW(lookup_gid("nonexisting_group"), LookupException);
}

TEST(lookup, nobody_groupname)
{
    try
    {
        ASSERT_NE(lookup_gid("nobody"), 0);
    }
    catch (const LookupException&)
    {
        try
        {
            ASSERT_NE(lookup_gid("nogroup"), 0);
        }
        catch (const LookupException&)
        {
            GTEST_SKIP() << "Neither nobody nor nogroup groups exists on this system";
            return;
        }
    }
}
} // namespace unittest
