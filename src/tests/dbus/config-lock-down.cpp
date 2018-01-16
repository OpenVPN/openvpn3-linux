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
 * @file   config-lock-down.cpp
 *
 * @brief  Simple test program to test flipping the locked_down attribute
 *         in a configuration profile
 */




#include <iostream>

#include "configmgr/proxy-configmgr.hpp"

int main(int argc, char **argv)
{
    if (argc != 2 and argc != 3)
    {
        std::cout << "Usage: " << argv[0] << "<configobject path> [yes|no]" << std::endl;
        return 1;
    }

    try
    {
        std::string cfgpath(argv[1]);
        OpenVPN3ConfigurationProxy cfgprx(G_BUS_TYPE_SYSTEM, cfgpath);

        std::cout << "Current lock-down setting: "
                  << (cfgprx.GetLockedDown() ? "true" : "false")
                  << std::endl;

        if (argc == 3)
        {
            std::string v(argv[2]);
            bool do_lock("yes" == v || "y" == v || "1" == v || "true" == v || "t" == v);
            cfgprx.SetLockedDown(do_lock);

            std::cout << "New lock-down setting: "
                      << (cfgprx.GetLockedDown() ? "true" : "false")
                      << std::endl;
        }

        return 0;
    }
    catch (std::exception& err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
