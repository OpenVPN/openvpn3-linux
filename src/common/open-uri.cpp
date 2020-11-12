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
 * @file   open-uri.cpp
 *
 * @brief  Opens URI addresses via the Glib2 GIO interface.  This is used
 *         to automatically open web based authentication URLs in a browser.
 */

#include <string>
#include <sstream>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include "open-uri.hpp"

OpenURIresult open_uri(const std::string& uri)
{
    GError *error = nullptr; // RIAA not possible: crosses initialization of ‘GError* error’
    OpenURIresult ret = NewOpenURIresult();
    char *scheme = g_uri_parse_scheme(uri.c_str());
    if (!scheme || 0 == scheme[0])
    {
        ret->status = OpenURIstatus::INVALID;
        ret->message = std::string("Could not parse URI: ") + uri;
        goto exit;
    }

    if (g_app_info_launch_default_for_uri(uri.c_str(), nullptr, &error))
    {
        ret->status = OpenURIstatus::SUCCESS;
    }
    else
    {
        ret->status = OpenURIstatus::FAIL;
        ret->message = std::string("Failed to open URI: ")
                     + std::string(error->message);
        g_clear_error(&error);
    }

 exit:
    g_free(scheme);
    return ret;
}
