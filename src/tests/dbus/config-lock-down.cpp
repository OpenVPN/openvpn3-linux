//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
    catch (std::exception &err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
