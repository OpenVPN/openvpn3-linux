//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-changetype.hpp
 *
 * @brief  Defines the network configuration change types the NetCfg service
 *         supports
 */

#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <gdbuspp/glib2/utils.hpp>


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

template <>
inline const char *glib2::DataType::DBus<NetCfgChangeType>() noexcept
{
    return glib2::DataType::DBus<std::uint32_t>();
}

template <>
inline NetCfgChangeType glib2::Value::Get<NetCfgChangeType>(GVariant *v) noexcept
{
    return static_cast<NetCfgChangeType>(glib2::Value::Get<uint32_t>(v));
}

template <>
inline NetCfgChangeType glib2::Value::Extract<NetCfgChangeType>(GVariant *v, int elm) noexcept
{
    return static_cast<NetCfgChangeType>(glib2::Value::Extract<uint32_t>(v, elm));
}

inline std::ostream &operator<<(std::ostream &os, const NetCfgChangeType &obj)
{
    return os << static_cast<uint32_t>(obj);
}


typedef std::map<std::string, std::string> NetCfgChangeDetails;

NetCfgChangeType operator|(const NetCfgChangeType &a, const NetCfgChangeType &b);
