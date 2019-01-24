//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  Arne Schwabe <arne@openvpn.net>
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
 * @file   overrides.cpp
 *
 * @brief  Code needed to handle configuration overrides
 */

#include <string>

#include "overrides.hpp"

const ValidOverride & GetConfigOverride(const std::string & key, bool ignoreCase)
{
    for (const auto& vo: configProfileOverrides)
    {
        if (vo.key==key)
        {
           return vo;
        }

        // This is appearently the best way to do a case insenstive
        // comparision in C++
        if (ignoreCase && std::equal(vo.key.begin(), vo.key.end(), key.begin(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b);} ))
        {
            return vo;
        }
    }

    // Override not found
    return invalidOverride;
}
