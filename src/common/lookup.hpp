//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   lookup.hpp
 *
 *
 * @brief  Various functions used to look up usernames, group names, uids
 *         and gids.
 */

#pragma once

#include <unistd.h>
#include <exception>

class LookupException : public std::exception
{
public:
    LookupException(const std::string& msg) : message(msg)
    {
    }

    const char* what() const noexcept
    {
        return message.c_str();
    }

    const std::string str() const noexcept
    {
        return message;
    }

#ifdef __GIO_TYPES_H__  // Only add GLib/GDBus methods if this is already use
    void SetDBusError(GDBusMethodInvocation *invocation) const noexcept
    {
        GError *dbuserr = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.lookup",
                                                          message.c_str());
        g_dbus_method_invocation_return_gerror(invocation, dbuserr);
        g_error_free(dbuserr);
    }
#endif

private:
    std::string message{};
};


std::string lookup_username(uid_t uid);
uid_t lookup_uid(std::string username);
uid_t get_userid(const std::string input);
gid_t lookup_gid(const std::string& groupname);
