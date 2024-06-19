//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <iostream>
#include <sysinfo.hpp>

int main()
{
    try
    {
        DevPosture::SysInfo info;
        std::cout << "Platform:\n"
                  << static_cast<std::string>(info) << "\n";

        DevPosture::DateTime dt;
        std::cout << "Date/time:\n"
                  << static_cast<std::string>(dt) << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception caught: " << e.what() << "\n";
        return 1;
    }
}
