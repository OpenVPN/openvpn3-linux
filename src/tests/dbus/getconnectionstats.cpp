//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
    for (auto& sd : session.GetConnectionStats())
    {
        std::cout << "  "
                  << sd.key
                  << std::setw(20-sd.key.size()) << std::setfill('.') << "."
                  << std::setw(12) << std::setfill('.')
                  << sd.value
                  << std::endl;
    }
    return 0;
}

