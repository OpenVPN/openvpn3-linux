//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
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



OpenURIresult open_uri(const std::string &uri)
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
