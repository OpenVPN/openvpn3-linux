//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
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
