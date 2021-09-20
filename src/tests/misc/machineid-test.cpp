//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2021         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2021         David Sommerseth <davids@openvpn.net>
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
    catch(const MachineIDException& excp)
    {
        std::cerr << "** ERROR **  " << excp.GetError() << std::endl;
    }
    return 0;
}


