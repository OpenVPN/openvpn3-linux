//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2021 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2021 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   machineid-test.cpp
 *
 * @brief  Simple stand-alone test program for MachineID
 */

#include <iostream>

#include "config.h"
#include "common/machineid.hpp"


int main()
{
    MachineID machine_id;

    try
    {
        std::cout << "Machine ID: " << machine_id << std::endl;
        machine_id.success();
    }
    catch (const MachineIDException &excp)
    {
        std::cerr << "** ERROR **  " << excp.GetError() << std::endl;
    }
    return 0;
}
