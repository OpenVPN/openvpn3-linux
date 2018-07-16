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
 * @file   log-prefix-selftest.cpp
 *
 * @brief  Simple test of the LogPrefix() function
 */

#include <iostream>

#include "config.h"
#include "log/log-helpers.hpp"

int main(int argc, char **argv)
{
    for (int group = 0; group <= LogGroupCount; group++)
    {
        for (int catg = 0; catg <= 9; catg++)
        {
            std::cout << "[" << group << ", " << catg << "] ";
            try {
                std::cout << LogPrefix((LogGroup) group, (LogCategory(catg)));
            }
            catch (LogException& e)
            {
                std::cout << "Failed: " << e.what();
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
