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
 * @file   backendstatus.hpp
 *
 * @brief  Declares the BackendStatus object, which includes a D-Bus GVariant
 *         value parser
 */

#ifndef SRC_CLIENT_BACKENDSTATUS_HPP_
#define SRC_CLIENT_BACKENDSTATUS_HPP_

#include "dbus/constants.hpp"

/**
 * Carries a status record as reported by a VPN backend client
 */
struct BackendStatus
{
    BackendStatus()
    {
        reset();
    }


    /**
     * Constructor which parses a GVariant dictionary containing a status
     * object
     *
     * @param status  Status object (GVariant *) to parse
     */
    BackendStatus(GVariant *status)
    {
        reset();
        Parse(status);
    }

    /**
     *  Resets the current status to UNSET
     */
    void reset()
    {
        major = StatusMajor::UNSET;
        major_str = "";
        minor = StatusMinor::UNSET;
        minor_str = "";
        message = "";
    }


    /**
     *   Parses a GVvariant dictionary containing the a status object
     *
     * @param status GVariant pointer to a valid dictionary containing the
     *               status object
     */
    void Parse(GVariant *status)
    {
        GVariant *d = nullptr;
        unsigned int v = 0;

        // FIXME: Should type-check better that the input GVariant
        //        contains the proper fields for a status object

        d = g_variant_lookup_value(status, "major", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        major = (StatusMajor) v;
        major_str = std::string(StatusMajor_str[v]);
        g_variant_unref(d);

        d = g_variant_lookup_value(status, "minor", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        minor = (StatusMinor) v;
        minor_str = std::string(StatusMinor_str[v]);
        g_variant_unref(d);

        gsize len;
        d = g_variant_lookup_value(status,
                                   "status_message", G_VARIANT_TYPE_STRING);
        message = std::string(g_variant_get_string(d, &len));
        g_variant_unref(d);
        if (len != message.size())
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed retrieving status message text "
                                "(inconsistent length)");
        }
    }

    StatusMajor major;
    std::string major_str;
    StatusMinor minor;
    std::string minor_str;
    std::string message;
};




#endif /* SRC_CLIENT_BACKENDSTATUS_HPP_ */
