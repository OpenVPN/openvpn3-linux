//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   timestamp.cpp
 *
 * @brief  Simple unit test for GetTimestamp()
 */

#include <time.h>
#include <gtest/gtest.h>
#include "common/timestamp.hpp"


namespace unittest {

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
} // namespace unittest
