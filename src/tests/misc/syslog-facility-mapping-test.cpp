//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
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
#include "log/logwriters/syslog.hpp"



int main()
{
    bool test_passed = true;
    try
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
            // clang-format on
        };

        for (auto const &fac : test_facilities)
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
            catch (SyslogException &excp)
            {
                if (fac.should_pass)
                {
                    test_passed = false;
                    std::cout << "FAIL: Syslog facility \"" << fac.name
                              << "\" returned an exception  "
                              << " but SHOULD have passed: " << excp.what()
                              << std::endl;
                    continue;
                }
                std::cout << "PASS: Tested: \"" << fac.name << "\""
                          << " threw a SyslogException: " << excp.what()
                          << std::endl;
            }
        }
    }
    catch (std::exception &excp)
    {
        std::cout << "Unexpected exception: " << excp.what() << std::endl;
        return 2;
    }

    std::cout << "All tests completed" << std::endl
              << std::endl;
    std::cout << "Test " << (test_passed ? "PASSED" : " ** FAILED **")
              << std::endl;
    return test_passed ? 0 : 1;
}
