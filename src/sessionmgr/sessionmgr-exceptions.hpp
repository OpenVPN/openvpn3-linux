//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   sessionmgr-exceptions.hpp
 *
 * @brief  Session Manager specific exception
 */

#pragma once

#include <string>
#include <exception>

#include "dbus/exceptions.hpp"

using namespace openvpn;

namespace SessionManager
{

#define THROW_SESSIONMGR(m) throw SessionManager::Exception(m, __FILE__, __LINE__, __FUNCTION__)
    class Exception : public DBusException
    {
    public:
        Exception(const std::string msg,
                                const char* file = __FILE__,
                                const int line = __LINE__,
                                const char* method = __FUNCTION__)
            : DBusException("SessionManager", msg, file, line, method)
        {
        }
    };
} // namespace SessionManager

