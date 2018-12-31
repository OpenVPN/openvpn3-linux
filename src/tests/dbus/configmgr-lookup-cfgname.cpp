//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN Inc <sales@openvpn.net>
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
 * @file   configmgr-lookup-cfgname.cpp
 *
 * @brief  Simple unit test, calling the
 *         net.openvpn.v3.configuration.LookupConfigName D-Bus method via
 *         an OpenVPN3ConfigurationProxy object
 */

#include <iostream>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"

int main(int argc, char **argv)
{
    if (2 < argc)
    {
        std::cerr << "Usage: " << argv[0] << " <config name>" << std::endl;
        return 1;
    }

    OpenVPN3ConfigurationProxy cfgmgr(G_BUS_TYPE_SYSTEM,
                                 OpenVPN3DBus_rootp_configuration);
    std::cout << "Lookup up configuration paths for '"
              << argv[1] << "'" << std::endl;
    unsigned int i = 0;
    for (const auto& p : cfgmgr.LookupConfigName(argv[1]))
    {
        std::cout << " - " << p << std::endl;
        ++i;
    }
    std::cout << "Found " << std::to_string(i) << " configuration"
              << (i != 1 ? "s" : "") << std::endl;
    return 0;
}


