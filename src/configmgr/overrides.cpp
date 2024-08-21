//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   overrides.cpp
 *
 * @brief  Code needed to handle configuration overrides
 */

#include <string>

#include "overrides.hpp"


std::optional<ValidOverride> GetConfigOverride(const std::string &key, bool ignoreCase)
{
    for (const auto &vo : configProfileOverrides)
    {
        if (vo.key == key)
        {
            return vo;
        }

        // This is appearently the best way to do a case insenstive
        // comparision in C++
        if (ignoreCase && std::equal(vo.key.begin(), vo.key.end(), key.begin(), [](char a, char b)
                                     {
                                         return std::tolower(a) == std::tolower(b);
                                     }))
        {
            return vo;
        }
    }

    // Override not found
    return {};
}
