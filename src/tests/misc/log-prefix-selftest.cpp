//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
            try
            {
                std::cout << LogPrefix((LogGroup)group, (LogCategory(catg)));
            }
            catch (LogException &e)
            {
                std::cout << "Failed: " << e.what();
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
