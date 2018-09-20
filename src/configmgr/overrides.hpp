//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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
 * @file   overrides.hpp
 *
 * @brief  Code needed to handle configuration overrides
 */

#pragma once

enum class OverrideType
{
    string,
    boolean,
    invalid
};

/**
 * Helper classes to store the list of overrides
 */
struct ValidOverride {
    ValidOverride(std::string key, OverrideType type)
        : key(key), type(type)
    {
    }


    inline bool valid() const
    {
        return type != OverrideType::invalid;
    }


    std::string key;
    OverrideType type;
};


struct OverrideValue {
    OverrideValue(const ValidOverride& override, bool value)
        : override(override), boolValue(value)
    {
    }


    OverrideValue(const ValidOverride& override, std::string value)
        : override(override), strValue(value)
    {
    }


    ValidOverride override;
    bool boolValue;
    std::string strValue;
};


const ValidOverride configProfileOverrides[] = {
    {"server-override", OverrideType::string},
    {"port-override", OverrideType::string},
    {"proto-override", OverrideType::string},
    {"ipv6", OverrideType::string},
    {"dns-fallback-google", OverrideType::boolean},
    {"dns-sync-lookup", OverrideType::boolean},
    {"auth-fail-retry", OverrideType::boolean},
    {"no-client-cert", OverrideType::boolean},
    {"allow-compression", OverrideType::string},
    {"force-cipher-aes-cbc", OverrideType::boolean},
    {"tls-version-min", OverrideType::string},
    {"tls-cert-profile", OverrideType::string},
    {"proxy-auth-cleartext", OverrideType::boolean}
};


const ValidOverride invalidOverride(std::string("invalid"), OverrideType::invalid);


const ValidOverride & GetConfigOverride(const std::string & key, bool ignoreCase=false)
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
