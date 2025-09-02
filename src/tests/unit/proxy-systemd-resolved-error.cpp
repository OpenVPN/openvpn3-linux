//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2025-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2025-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file   src/tests/unit/proxy-systemd-resolved-error.cpp
 *
 *  @brief  Unit test for NetCfg::DNS::resolved::Error
 */

#include <gtest/gtest.h>

#include "netcfg/dns/proxy-systemd-resolved.hpp"

using namespace NetCfg::DNS::resolved;

TEST(proxy_systemd_resolved, Errors_Storage)
{
    auto errors = Error::Storage::Create();
    EXPECT_EQ(errors->GetLinks().size(), 0);

    errors->Add("/link/1", "TestMethod_A", "Test error message 1.A");
    EXPECT_EQ(errors->GetLinks().size(), 1);

    errors->Add("/link/2", "AnotherTestMethod", "Test error message 2.A");
    ASSERT_EQ(errors->GetLinks().size(), 2);

    errors->Add("/link/1", "TestMethod_B", "Test error message 1.B");
    EXPECT_EQ(errors->GetLinks().size(), 2);
    EXPECT_EQ(errors->NumErrors("/link/1"), 2);
    EXPECT_EQ(errors->NumErrors("/link/2"), 1);

    Error::Message::List link1_err = errors->GetErrors("/link/1");
    ASSERT_EQ(link1_err.size(), 2);
    EXPECT_EQ(errors->NumErrors("/link/1"), 0);
    ASSERT_EQ(errors->GetLinks().size(), 1);
    EXPECT_STREQ(link1_err[0].method.c_str(), "TestMethod_A");
    EXPECT_STREQ(link1_err[0].message.c_str(), "Test error message 1.A");
    EXPECT_STREQ(link1_err[0].str().c_str(), "[TestMethod_A] Test error message 1.A");
    EXPECT_STREQ(link1_err[1].method.c_str(), "TestMethod_B");
    EXPECT_STREQ(link1_err[1].message.c_str(), "Test error message 1.B");
    EXPECT_STREQ(link1_err[1].str().c_str(), "[TestMethod_B] Test error message 1.B");

    EXPECT_EQ(errors->NumErrors("/link/2"), 1);
    auto link2_err = errors->GetErrors("/link/2");
    ASSERT_EQ(link2_err.size(), 1);
    EXPECT_EQ(errors->NumErrors("/link/2"), 0);
    ASSERT_EQ(errors->GetLinks().size(), 0);
    EXPECT_STREQ(link2_err[0].str().c_str(), "[AnotherTestMethod] Test error message 2.A");
}
