//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   logtag.cpp
 *
 * @brief  Implementation of the LogTag class
 */

#include <string>
#include "logtag.hpp"

//
//  LogTag class implementation
//

LogTag::LogTag(std::string sender, std::string interface)
{
    tag = std::string("[") + sender + "/" + interface + "]";

    // Create a hash of the tag, used as an index
    std::hash<std::string> hashfunc;
    hash = hashfunc(tag);
}


std::string LogTag::str() const
{
    return std::string("{tag:") + std::string(std::to_string(hash))
           + std::string("}");
}
