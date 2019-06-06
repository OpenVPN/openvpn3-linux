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
 * @file   syslog-facility-mapping-test.cpp
 *
 * @brief  Simple test of SyslogWriter::ConvertLogFacility()
 */


#include <iostream>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "log/logwriter.hpp"

namespace unittest {

TEST(log, syslog_facility_mapping)
{
    struct log_facility_mapping_t {
        const std::string name;
        const int facility;
        const bool should_pass;
    };

    //  Various inputs to test, with their expected test result
    static const struct log_facility_mapping_t test_facilities[] =
        {
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
