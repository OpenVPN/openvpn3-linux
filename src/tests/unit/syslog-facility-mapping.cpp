//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   syslog-facility-mapping-test.cpp
 *
 * @brief  Simple test of SyslogWriter::ConvertLogFacility()
 */


#include <iostream>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "log/logwriter.hpp"
#include "log/logwriters/syslog.hpp"

namespace unittest {

TEST(log, syslog_facility_mapping)
{
    struct log_facility_mapping_t
    {
        const std::string name;
        const int facility;
        const bool should_pass;
    };

    //  Various inputs to test, with their expected test result
    static const struct log_facility_mapping_t test_facilities[] = {
        // clang-format off
            {"LOG_AUTH",     LOG_AUTH,     true},
            {"LOG_AUTHPRIV", LOG_AUTHPRIV, true},
            {"LOG_CRON",     LOG_CRON,     true},
            {"LOG_DAEMON",   LOG_DAEMON,   true},
            {"LOG_FTP",      LOG_FTP,      true},
            {"LOG_KERN",     LOG_KERN,     true},
            {"LOG_LOCAL0",   LOG_LOCAL0,   true},
            {"LOG_LOCAL1",   LOG_LOCAL1,   true},
            {"LOG_LOCAL2",   LOG_LOCAL2,   true},
            {"LOG_LOCAL3",   LOG_LOCAL3,   true},
            {"LOG_LOCAL4",   LOG_LOCAL4,   true},
            {"LOG_LOCAL5",   LOG_LOCAL5,   true},
            {"LOG_LOCAL6",   LOG_LOCAL6,   true},
            {"LOG_LOCAL7",   LOG_LOCAL7,   true},
            {"LOG_LPR",      LOG_LPR,      true},
            {"LOG_MAIL",     LOG_MAIL,     true},
            {"LOG_NEWS",     LOG_NEWS,     true},
            {"LOG_SYSLOG",   LOG_SYSLOG,   true},
            {"LOG_USER",     LOG_USER,     true},
            {"LOG_UUCP",     LOG_UUCP,     true},
            {"LOG_INVALID",  -1,           false},
            {"",             -1,           false}
            // clang-format off
        };


    for (auto const& fac : test_facilities)
    {
        try
        {
            int r = SyslogWriter::ConvertLogFacility(fac.name);
            if (fac.should_pass)
            {
                ASSERT_GE(r, 0) << "Unexpected return value, should be >= 0";
            }
            else
            {
                ASSERT_LT(r, 0) << "Unexpected return value, should be < 0";
            }
            ASSERT_EQ(fac.facility, r) << "Invalid facility map value";
        }
        catch (SyslogException& excp)
        {
            ASSERT_FALSE(fac.should_pass);
        }
    }
}

} // namespace unittest
