//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   gettimestamp.cpp
 *
 * @brief  Tests the GetTimestamp() function
 */

#include <iostream>

#include "common/timestamp.hpp"


int main()
{
    std::cout << "Current Timestamp: " << GetTimestamp() << std::endl;
    return 0;
}
