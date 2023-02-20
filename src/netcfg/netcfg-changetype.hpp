//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-changetype.hpp
 *
 * @brief  Defines the network configuration change types the NetCfg service
 *         supports
 */

#pragma once

#include <string>
#include <map>


enum class NetCfgChangeType : std::uint16_t
{
    // clang-format off
    UNSET              = 0,         //      0
    DEVICE_ADDED       = 1,         //      1
    DEVICE_REMOVED     = 1 <<  1,   //      2
    IPADDR_ADDED       = 1 <<  2,   //      4
    IPADDR_REMOVED     = 1 <<  3,   //      8
    ROUTE_ADDED        = 1 <<  4,   //     16
    ROUTE_REMOVED      = 1 <<  5,   //     32
    ROUTE_EXCLUDED     = 1 <<  6,   //     64
    DNS_SERVER_ADDED   = 1 <<  7,   //    128
    DNS_SERVER_REMOVED = 1 <<  8,   //    256
    DNS_SEARCH_ADDED   = 1 <<  9,   //    512
    DNS_SEARCH_REMOVED = 1 << 10,   //   1024
    // clang-format on
};

typedef std::map<std::string, std::string> NetCfgChangeDetails;

NetCfgChangeType operator|(const NetCfgChangeType &a, const NetCfgChangeType &b);
