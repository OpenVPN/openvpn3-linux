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
#include <termios.h>

#include <glib.h>

#include "config.h"
#ifdef HAVE_CONFIG_VERSION_H
#include "config-version.h"
#endif

#include "utils.hpp"

#include <openvpn/legal/copyright.hpp>
#include <openvpn/common/platform_string.hpp>


#ifndef CONFIGURE_GIT_REVISION
constexpr char package_version_str[] = PACKAGE_GUIVERSION;
#else
constexpr char package_version_str[] = "git:" CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS;
#endif

const char * package_version()
{
    return package_version_str;
}

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

    ver << PACKAGE_NAME << " " << package_version_str;

    //  Simplistic basename() approach, extracting just the filename
    //  of the binary from argv[0]
    ver << " (" << simple_basename(component) << ")"
        << std::endl;

#if defined(OPENVPN_TUN_BUILDER_BASE_H)
    ver << ClientAPI::OpenVPNClient::platform() << std::endl;
#else
    ver << openvpn::platform_string();
#if defined(OPENVPN_DEBUG)
    ver << " built on " __DATE__ " " __TIME__;
#endif // OPENVPN_DEBUG
#endif // OPENVPN_TUN_BUILDER_BASE_H
    ver << std::endl << openvpn_copyright;

    return ver.str();
}


const std::string get_guiversion()
{
#ifdef CONFIGURE_GIT_REVISION
    return openvpn::platform_string(PACKAGE_NAME, "git:" CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS);
#else
    return openvpn::platform_string(PACKAGE_NAME, PACKAGE_GUIVERSION);
#endif
}

/**
 *  GLib2 interrupt/signal handler.  This is used to gracefully shutdown
 *  the GLib main loop.  This is called via the g_unix_signal_add() or
 *  similar interfaces which has access to the GMainLoop object.
 *
 * @param loop  GMainLoop object which is to be shut down.
 * @return See @GSourceFunc() declaration in glib2.  We return
 *         G_SOURCE_CONTINUE as we do not want to remove/disable the
 *         signal processing.
 */
int stop_handler(void *loop)
{
#ifdef SHUTDOWN_NOTIF_PROCESS_NAME
    std::cout << "** Shutting down ";
    std::cout << SHUTDOWN_NOTIF_PROCESS_NAME << " ";
    std::cout << "(pid: " << std::to_string(getpid()) << ")" << std::endl;
#endif
    g_main_loop_quit((GMainLoop *)loop);
    return G_SOURCE_CONTINUE;
}


/**
 *  Enables or disables the terminal input echo flag.  This
 *  is used to mask password input.
 *
 * @param echo  Boolean, if true the console input will be echoed to console
 */
void set_console_echo(bool echo)
{
    struct termios console;
    tcgetattr(STDIN_FILENO, &console);
    if (echo)
    {
        console.c_lflag |= ECHO;
    }
    else
    {
        console.c_lflag &= ~ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &console);
}
