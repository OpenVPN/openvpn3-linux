//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN, Inc. <davids@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_UTILS_HPP
#define OPENVPN3_UTILS_HPP

#include <iostream>
#include <string>
#include <cstring>
#include <exception>

#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

#include <glib.h>
#include <glib-unix.h>

#include "config.h"


void drop_root()
{
    if (getegid() == 0)
    {
        std::cout << "[INFO] Dropping root group privileges to " << OPENVPN_GROUP << std::endl;

        // Retrieve the group information
        errno = 0;
        struct group *groupinfo = getgrnam(OPENVPN_GROUP);
        if (NULL == groupinfo)
        {
            if (errno == 0)
            {
                throw std::runtime_error("Could not find the group ID for " + std::string(OPENVPN_GROUP));
            }
            throw std::runtime_error("An error occured while calling getgrnam():"
                                     + std::string(strerror(errno)));
        }
        gid_t gid = groupinfo->gr_gid;
        if (-1 == setresgid(gid, gid, gid))
        {
            throw std::runtime_error("Could not set the new group ID (" + std::to_string(gid) + ") "
                                     + "for the user " + std::string(OPENVPN_GROUP));
        }
    }

    if (geteuid() == 0)
    {
        std::cout << "[INFO] Dropping root user privileges to "<< OPENVPN_USERNAME << std::endl;

        // Retrieve the user information
        struct passwd *userinfo = getpwnam(OPENVPN_USERNAME);
        if (NULL == userinfo)
        {
            if (errno == 0)
            {
                throw std::runtime_error("Could not find the User ID for " + std::string(OPENVPN_USERNAME));
            }
            throw std::runtime_error("An error occured while calling getgrnam():"
                                     + std::string(strerror(errno)));
        }

        uid_t uid = userinfo->pw_uid;
        if (-1 == setresuid(uid, uid, uid))
        {
            throw std::runtime_error("Could not set the new user ID (" + std::to_string(uid) + ") "
                                + "for the user " + OPENVPN_USERNAME);
        }
    }
}



int stop_handler(void *loop)
{
#if 1
    std::cout << "** Shutting down (pid: " << std::to_string(getpid()) << ")" << std::endl;
#endif
    g_main_loop_quit((GMainLoop *)loop);
    return true;
}

#endif // OPENVPN3_UTILS_HPP
