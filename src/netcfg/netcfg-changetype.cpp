//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-changetype.cpp
 *
 * @brief  Implements support functions needed by NetCfgChangeType
 */

#include "netcfg-changetype.hpp"


NetCfgChangeType operator|(const NetCfgChangeType &a, const NetCfgChangeType &b)
{
    return NetCfgChangeType(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
}
