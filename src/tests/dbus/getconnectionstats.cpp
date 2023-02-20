//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   getconnectionstats.cpp
 *
 * @brief  Simple client which queries an existing VPN session for its
 *         connection statistics and dumps that to stdout
 */

#include <iostream>
#include <iomanip>

#include "sessionmgr/proxy-sessionmgr.hpp"

using namespace openvpn;



int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <session path>" << std::endl;
        return 1;
    }

    auto session = OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, std::string(argv[1]));
    for (auto &sd : session.GetConnectionStats())
    {
        std::cout << "  "
                  << sd.key
                  << std::setw(20 - sd.key.size()) << std::setfill('.') << "."
                  << std::setw(12) << std::setfill('.')
                  << sd.value
                  << std::endl;
    }
    return 0;
}
