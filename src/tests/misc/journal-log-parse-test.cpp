//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
//

/**
 * @file journal-log-parse-test.cpp
 *
 * @brief  Simple command line tool to test the Log::Journald::Parser API
 *
 */
#include <iostream>
#include "log/journal-log-parse.hpp"

using namespace Log::Journald;

int main(int argc, char **argv)
{
    Parse jrn;

    if ((argc - 1) % 2 != 0)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <filter_mode> <filter_value> [<filter_mode> <filter_value>...]"
                  << std::endl;
        std::cerr << std::endl
                  << "filter_mode = timestamp, logtag, session-token, path, sender, interface"
                  << std::endl
                  << std::endl;
        return 1;
    }

    for (int i = 1; i < argc;)
    {
        std::string mode(argv[i]);
        std::string value(argv[i + 1]);
        i += 2;

        Parse::FilterType ft = {};
        if ("timestamp" == mode)
        {
            ft = Parse::FilterType::TIMESTAMP;
        }
        else if ("logtag" == mode)
        {
            ft = Parse::FilterType::LOGTAG;
        }
        else if ("session-token" == mode)
        {
            ft = Parse::FilterType::SESSION_TOKEN;
        }
        else if ("path" == mode)
        {
            ft = Parse::FilterType::OBJECT_PATH;
        }
        else if ("sender" == mode)
        {
            ft = Parse::FilterType::SENDER;
        }
        else if ("interface" == mode)
        {
            ft = Parse::FilterType::INTERFACE;
        }
        else
        {
            std::cout << "** ERROR **   Unknown filter mode: " << mode << std::endl;
            return 2;
        }
        jrn.AddFilter(ft, value);
    }

    LogEntries l = jrn.Retrieve();
    // std::cout << l;
    std::cout << l.GetJSON() << std::endl;
}