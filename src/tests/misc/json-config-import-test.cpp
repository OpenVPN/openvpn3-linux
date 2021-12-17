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
    Json::Value jsondata;
    if (argc < 2)
    {
        std::cerr << "Reading configuration from stdin" << std::endl;

        std::stringstream cfgfile;
        for (std::string line; std::getline(std::cin, line);)
        {
            cfgfile << line << std::endl;
        }
        cfgfile >> jsondata;
    }
    else
    {
        std::cerr << "Reading configuration from " << argv[1] << std::endl;
        std::ifstream file(argv[1], std::ifstream::binary);
        file >> jsondata;
    }

    OptionListJSON opts;
    opts.json_import(jsondata);
    std::cout << opts.string_export() << std::endl;
    return 0;
}
