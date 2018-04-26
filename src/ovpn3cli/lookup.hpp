//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @brief  [enter description of what this file contains]
 */

#ifndef OPENVPN3_OVPN3CLI_LOOKUP_HPP
#define OPENVPN3_OVPN3CLI_LOOKUP_HPP

#include <cstring>
#include <sys/types.h>
#include <pwd.h>

#include "common/utils.hpp"

/**
 *  Looks up the uid of a user account to extract its username
 *
 * @param uid   uid_t to use for the query
 * @return      Returns a std::string containing the username on success,
 *              otherwise the uid is returned as a string, encapsulated by ().
 */
static std::string lookup_username(uid_t uid)
{
    struct passwd pwrec;
    struct passwd *result = nullptr;
    size_t buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
    char *buf = nullptr;

    buf = (char *) malloc(buflen);
    memset(buf, 0, buflen);

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
 * @param username  std::string containing the usrename to lookup
 * @return An uid_t integer is returned on success.
 */
static uid_t lookup_uid(std::string username)
{
    struct passwd pwrec;
    struct passwd *result = nullptr;
    size_t buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
    char *buf = nullptr;

    buf = (char *) malloc(buflen);
    memset(buf, 0, buflen);
    uid_t ret;
    int r = getpwnam_r(username.c_str(), &pwrec, buf, buflen, &result);
    if ( (0 == r) && (NULL != result))
    {

        ret = result->pw_uid;
    }
    else
    {
        ret =  -1;
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

 * @return Returns a uid_t representation of the username or uid.  If username
 *         lookup fails, it will return -1;
 */
inline static uid_t get_userid(const std::string input)
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


#endif // OPENVPN3_OVPN3CLI_LOOKUP_HPP

