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
 * @file   timestamp.cpp
 *
 * @brief  Simple unit test for GetTimestamp()
 */

#include <time.h>
#include <gtest/gtest.h>
#include "common/timestamp.hpp"


namespace unittest
{

TEST(common, GetTimestamp)
{
    char buf[200];
    time_t t;
    struct tm *tmp = nullptr;

    // Retrieve time stamp, the C way
    t = time(NULL);
    tmp = localtime(&t);

    // Retrieve time stamp via the C++ implementation
    std::string tstamp(GetTimestamp());

    // Format the timestamp from the "C way"
    ASSERT_GT(strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S ", tmp), 0)
        << "strftime() failed";

    // Compare results, which is expected to be identical
    std::string cmp(buf);
    ASSERT_EQ(tstamp, cmp) << "Mismatch between C and C++ implementation";
}
}
