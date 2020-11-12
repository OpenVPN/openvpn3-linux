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
 * @file   open-uri.hpp
 *
 * @brief  Opens URI addresses via the Glib2 GIO interface.  This is used
 *         to automatically open web based authentication URLs in a browser.
 */


#pragma once

#include <memory>

enum class OpenURIstatus : std::uint8_t {
    UNKNOWN,            /**< Status not known */
    SUCCESS,            /**< URI was opened successfully */
    FAIL,               /**< Did not manage to open the URI in any browser */
    INVALID             /**< URI is invalid/could not parse the URI */
};


struct OpenURIresult_
{
    OpenURIstatus status;
    std::string message;

    OpenURIresult_()
        : status(OpenURIstatus::UNKNOWN),
          message()
    {
    }

};
typedef std::unique_ptr<OpenURIresult_> OpenURIresult;
static inline OpenURIresult NewOpenURIresult()
{
    OpenURIresult r;
    r.reset(new OpenURIresult_());
    return r;
}

/**
 *  Opens the provided URI (typically an HTTP/HTTPS address) via the
 *  preferred program configured on the system
 *
 *  @param uri    std::string containing the complete URI/URL to open
 *
 *  @return Returns OpenURIresult with a result of the success or indication
 *          of what failed
 */
OpenURIresult open_uri(const std::string& uri);
