//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-changetype.hpp
 *
 * @brief  Defines the network configuration change types the NetCfg service
 *         supports
 */


#pragma once

#include <string>
#include <map>

enum class NetCfgChangeType : std::uint16_t {
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
};

typedef std::map<std::string, std::string> NetCfgChangeDetails;

NetCfgChangeType operator | (const NetCfgChangeType& a, const NetCfgChangeType& b);
