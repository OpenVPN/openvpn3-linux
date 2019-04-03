//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
#include "log/logwriter.hpp"

int main()
{
    bool test_passed = true;
    try
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
                if (!fac.should_pass || r < 0)
                {
                    test_passed = false;
                    std::cout << "FAIL: Syslog facility \"" << fac.name
                              << "\" returned " << std::to_string(r)
                              << " but should NOT have passed"
                              << std::endl;
                    continue;
                }

                if (fac.facility != r)
                {
                    test_passed = false;
                    std::cout << "FAIL: Syslog facility \"" << fac.name
                              << "\" returned " << std::to_string(r)
                              << " which is wrong.  Correct value is:"
                              << fac.facility
                              << std::endl;
                }

                std::cout << "PASS: Tested: \"" << fac.name << "\""
                              << " == " << fac.facility
                              << std::endl;

            }
            catch (SyslogException& excp)
            {
                if (fac.should_pass)
                {
                    test_passed = false;
                    std::cout << "FAIL: Syslog facility \"" << fac.name
                              << "\" returned an excpetion  "
                              << " but SHOULD have passed: " << excp.what()
                              << std::endl;
                    continue;
                }
                std::cout << "PASS: Tested: \"" << fac.name << "\""
                              << " threw a SyslogException: " << excp.what()
                              << std::endl;
            }
        }
    } catch (std::exception& excp)
    {
        std::cout << "Unexpected exception: " << excp.what() << std::endl;
        return 2;
    }

    std::cout << "All tests completed" << std::endl << std::endl;
    std::cout << "Test " << (test_passed ? "PASSED" : " ** FAILED **")
              << std::endl;
    return test_passed ? 0 : 1;
}

