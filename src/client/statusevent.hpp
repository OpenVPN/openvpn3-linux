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
 * @brief  Declares the StatusEvent object, which includes a D-Bus GVariant
 *         value parser
 */

#pragma once

#include "dbus/constants.hpp"

/**
 * Carries a status event record as reported by a VPN backend client
 */
struct StatusEvent
{
    StatusEvent()
    {
        reset();
    }


    /**
     * Constructor which parses a GVariant dictionary containing a status
     * object
     *
     * @param status  Status object (GVariant *) to parse
     */
    StatusEvent(GVariant *status)
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
        minor = StatusMinor::UNSET;
        message.clear();
    }


    /**
     *  Checks if the StatusEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty()
    {
        return (StatusMajor::UNSET == major)
               && (StatusMinor::UNSET == minor)
               && message.empty();
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
        g_variant_unref(d);

        d = g_variant_lookup_value(status, "minor", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        minor = (StatusMinor) v;
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


    /**
     *  Makes it possible to write StatusEvent in a readable format
     *  via iostreams, such as 'std::cout << status', where status is a
     *  StatusEvent object.
     *
     * @param os  std::ostream where to write the data
     * @param ev  StatusEvent to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded StatusEvent information
     */
    friend std::ostream& operator<<(std::ostream& os , const StatusEvent& s)
    {
        if ((StatusMajor::UNSET == s.major)
            && (StatusMinor::UNSET == s.minor)
            && s.message.empty())
        {
            return os << "(No status)";
        }
        else
        {
            return os << "[" << std::to_string((unsigned) s.major) << ","
                             << std::to_string((unsigned) s.minor)
                      << "] " << StatusMajor_str[(unsigned) s.major] << ", "
                              << StatusMinor_str[(unsigned) s.minor]
                      << (!s.message.empty() ? ": " : "")
                      << (!s.message.empty() ? s.message : "");
        }
    }

    StatusMajor major;
    StatusMinor minor;
    std::string message;
};
