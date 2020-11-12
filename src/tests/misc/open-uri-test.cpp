//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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



