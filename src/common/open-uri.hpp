//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   open-uri.hpp
 *
 * @brief  Opens URI addresses via the Glib2 GIO interface.  This is used
 *         to automatically open web based authentication URLs in a browser.
 */

#pragma once

#include <cstdint>
#include <memory>


enum class OpenURIstatus : std::uint8_t
{
    UNKNOWN, /**< Status not known */
    SUCCESS, /**< URI was opened successfully */
    FAIL,    /**< Did not manage to open the URI in any browser */
    INVALID  /**< URI is invalid/could not parse the URI */
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
OpenURIresult open_uri(const std::string &uri);
