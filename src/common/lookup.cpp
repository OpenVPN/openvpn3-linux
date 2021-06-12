//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   lookup.cpp
 *
 * @brief  Various functions used to look up usernames, group names, uids
 *         and gids.
 */

#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "lookup.hpp"

/**
 *  Checks if the input string is a number or a string
 *
 * @param data  std::string of the data to classify
 * @return Returns true if the input string is a plain, positive integer
 *         number, otherwise false.
 */
static inline bool isanum_string(const std::string& data)
{
    return std::all_of(data.begin(), data.end(), ::isdigit);
}



static inline char* alloc_sysconf_buffer(int name, size_t* retlen)
{
    long buflen = sysconf(name);
    if (buflen == -1)
    {
        // Some Linux distributions/libc libraries may fail to lookup the
        // the proper sysconf() value.  Fallback to a reasonable size most
        // likely large enough.  This is a known issue with Alpine Linux.
        buflen = 16384;
    }
    *retlen = buflen;
    char *retbuf = (char *)malloc((size_t) buflen);
    memset(retbuf, 0, buflen);
    return retbuf;
}


/**
 *  Looks up the uid of a user account to extract its username
 *
 * @param uid   uid_t to use for the query
 * @return      Returns a std::string containing the username on success,
 *              otherwise the uid is returned as a string, encapsulated by ().
 */
std::string lookup_username(uid_t uid)
{
    struct passwd pwrec;
    struct passwd *result = nullptr;
    size_t buflen = 0;
    char *buf = alloc_sysconf_buffer(_SC_GETPW_R_SIZE_MAX, &buflen);

    std::string ret;
    int r = getpwuid_r(uid, &pwrec, buf, buflen, &result);
    if ( (0 == r) && (NULL != result))
    {
        ret = std::string(pwrec.pw_name);
    }
    else
    {
        ret = "(" + std::to_string(uid) + ")";
    }
    free(buf);
    return ret;
}


/**
 *  Looks up a specific uid_t based on the provided username.
 *
 * @param username  std::string containing the username to lookup
 * @return An uid_t integer is returned on success
 * @throws LookupException if username is not found
 */
uid_t lookup_uid(std::string username)
{
    struct passwd pwrec;
    struct passwd *result = nullptr;
    size_t buflen = 0;
    char *buf = alloc_sysconf_buffer(_SC_GETPW_R_SIZE_MAX, &buflen);

    uid_t ret;
    int r = getpwnam_r(username.c_str(), &pwrec, buf, buflen, &result);
    if ( (0 == r) && (NULL != result))
    {

        ret = result->pw_uid;
    }
    else
    {
        free(buf);
        throw LookupException("User '" + username + "' not found");
    }
    free(buf);
    return ret;
}


/**
 *  Simple helper function which returns the uid_t value of the input
 *  whether the input string is an numeric value (uid) or a username which
 *  goes through a uid lookup.
 *
 * @param input  std::string containing a username or a uid

 * @return Returns a uid_t representation of the username or uid.
 * @throws LookupException if username is not found
 */
uid_t get_userid(const std::string input)
{
    // If the argument is not a number, we consider it
    // a username.  Lookup the UID for this username
    if (!isanum_string(input))
    {
        return lookup_uid(input);
    }
    else
    {
        return std::stoi(input);
    }
}


/**
 *  Looks up a specific gid_t based on the provided group name.
 *
 * @param groupname  std::string containing the group name to lookup
 * @return An gid_t integer is returned on success
 * @throws LookupException if group is not found
 */
gid_t lookup_gid(const std::string& groupname)
{
    struct group grprec;
    struct group *result = nullptr;
    size_t buflen = 0;
    char *buf = alloc_sysconf_buffer(_SC_GETPW_R_SIZE_MAX, &buflen);

    gid_t ret;
    int r = getgrnam_r(groupname.c_str(), &grprec, buf, buflen, &result);
    if ( (0 == r) && (NULL != result))
    {
        ret = result->gr_gid;
    }
    else
    {
        free(buf);
        throw LookupException("Group '" + groupname + "' not found");
    }

    free(buf);
    return ret;
}
