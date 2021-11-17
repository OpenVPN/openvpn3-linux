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
 * @file   config-export-json-test.cpp
 *
 * @brief  Simple test program reading an OpenVPN configuration file
 *         from stdin, parsing it with OptionListJSON and exporting it
 *         to stdout as JSON.
 */

#include <iostream>
#include <sstream>

#include <openvpn/log/logsimple.hpp>
#include "common/core-extensions.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{

    std::stringstream conf;
    if (argc < 2)
    {
        std::cerr << "Reading configuration from stdin" << std::endl;

        for (std::string line; std::getline(std::cin, line);) {
            conf << line << std::endl;
        }
    }
    else
    {
        std::cerr << "Reading configuration from " << argv[1] << std::endl;
        std::ifstream file(argv[1], std::ifstream::binary);
        conf << file.rdbuf();
    }

    ProfileMergeFromString pm(conf.str(), "",
                              ProfileMerge::FOLLOW_FULL,
                              ProfileParseLimits::MAX_LINE_SIZE,
                              ProfileParseLimits::MAX_PROFILE_SIZE);

    OptionList::Limits limits("profile is too large",
                      ProfileParseLimits::MAX_PROFILE_SIZE,
                      ProfileParseLimits::OPT_OVERHEAD,
                      ProfileParseLimits::TERM_OVERHEAD,
                      ProfileParseLimits::MAX_LINE_SIZE,
                      ProfileParseLimits::MAX_DIRECTIVE_SIZE);
    OptionListJSON options;
    options.parse_from_config(pm.profile_content(), &limits);
    std::cout << options.json_export() << std::endl;
    return 0;
}
