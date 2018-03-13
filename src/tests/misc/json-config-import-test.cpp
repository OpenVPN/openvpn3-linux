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
 * @file   json-config-import-test.cpp
 *
 * @brief  Simple test program reading a JSON blob from stdin, parses it
 *         with the ProfileMergeJSON class and exports the parsed
 *         configuration as a text/plain OpenVPN configuration profile.
 */

#include <iostream>
#include <sstream>

#include <openvpn/log/logsimple.hpp>

#include "common/core-extensions.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
    std::stringstream jsoncfg;

    for (std::string line; std::getline(std::cin, line);) {
        jsoncfg << line << std::endl;
    }

    ProfileMergeJSON pm(jsoncfg.str());
    std::cout << pm.profile_content() << std::endl;
    return 0;
}
