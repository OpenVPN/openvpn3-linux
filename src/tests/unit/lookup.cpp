//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
 * @file   lookup-tests.cpp
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

TEST(common, lookup_tests)
{
    uid_t root = lookup_uid("root");
    uid_t nobody = lookup_uid("nobody");
    uid_t invalid = lookup_uid("nonexiting_user");

    ASSERT_EQ(root, 0);
    ASSERT_EQ(invalid, -1);

    std::string root_username = lookup_username(root);
    ASSERT_EQ(root_username, "root");

    // The 'nobody' user is not available when building
    // on SUSE via docker, so we expect a different output
    // in this case.  -1 == 4294967295 as unsigned int.
    std::string nobody_username = lookup_username(nobody);
    if (nobody < 4294967295 || nobody > -1)
    {
        ASSERT_EQ(nobody_username, "nobody");
    }
    else
    {
        ASSERT_EQ(nobody_username, "(4294967295)");
    }
}
} // namespace unittest
