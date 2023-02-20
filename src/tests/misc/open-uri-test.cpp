//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   open-uri-test.cpp
 *
 * @brief  Test program for the open_uri() function
 */

#include <iostream>
#include "common/open-uri.hpp"


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <URL to open>" << std::endl;
        return 1;
    }

    OpenURIresult res = open_uri(std::string(argv[1]));
    switch (res->status)
    {
    case OpenURIstatus::INVALID:
    case OpenURIstatus::FAIL:
        std::cerr << "** ERROR **  " << res->message << std::endl;
        return 2;

    case OpenURIstatus::SUCCESS:
        std::cout << "URL opened successfully" << std::endl;
        return 0;

    default:
        std::cerr << "** ????? ** Unknown status" << std::endl;
        return 3;
    }
}
