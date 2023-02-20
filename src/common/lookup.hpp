//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
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
    LookupException(const std::string &msg)
        : message(msg)
    {
    }

    const char *what() const noexcept
    {
        return message.c_str();
    }

    const std::string str() const noexcept
    {
        return message;
    }

#ifdef __GIO_TYPES_H__ // Only add GLib/GDBus methods if this is already use
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
gid_t lookup_gid(const std::string &groupname);
