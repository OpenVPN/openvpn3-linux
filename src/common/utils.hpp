//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#ifndef OPENVPN3_UTILS_HPP
#define OPENVPN3_UTILS_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>

#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

#include <glib.h>
#include <glib-unix.h>

#include <openvpn/legal/copyright.hpp>

#include "config.h"
#ifdef HAVE_CONFIG_VERSION_H
#include "config-version.h"
#endif


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
            throw std::runtime_error("An error occurred while calling getgrnam():"
                                     + std::string(strerror(errno)));
        }
        gid_t gid = groupinfo->gr_gid;
        if (-1 == setresgid(gid, gid, gid))
        {
            throw std::runtime_error("Could not set the new group ID (" + std::to_string(gid) + ") "
                                     + "for the user " + std::string(OPENVPN_GROUP));
        }

        // Remove any potential supplementary groups
        if (-1 == setgroups(0, NULL))
        {
            throw std::runtime_error("Could not remove supplementary groups");
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
            throw std::runtime_error("An error occurred while calling getgrnam():"
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


std::string simple_basename(const std::string filename)
{
    return filename.substr(filename.rfind('/')+1);
}



/**
 *  Checks if the input string is a number or a string
 *
 * @param data  std::string of the data to classify
 * @return Returns true if the input string is a plain, positive integer
 *         number, otherwise false.
 */
bool isanum_string(const std::string& data)
{
    return std::all_of(data.begin(), data.end(), ::isdigit);
}


/**
 *  Returns a string containing a version reference of the build.
 *  If a git checkout is discovered, flags identifying if there are
 *  uncommitted changes will be present too.
 *
 * @param component  An additional string identifying which component this
 *                   version reference belongs to.  Normally argv[0].
 *
 * @return A pre-formatted std::string containing the version references
 */
std::string get_version(std::string component)
{
    std::stringstream ver;

#ifndef CONFIGURE_GIT_REVISION
    ver << PACKAGE_NAME << " " << PACKAGE_GUIVERSION;
#else
    ver << PACKAGE_NAME << " "
        << "git:" << CONFIGURE_GIT_REVISION << CONFIGURE_GIT_FLAGS;
#endif

    //  Simplistic basename() approach, extracting just the filename
    //  of the binary from argv[0]
    ver << " (" << simple_basename(component) << ")"
        << std::endl;

#ifdef OPENVPN_TUN_BUILDER_BASE_H
    ver << ClientAPI::OpenVPNClient::platform() << std::endl;
#endif
    ver << openvpn_copyright;

    return ver.str();
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
