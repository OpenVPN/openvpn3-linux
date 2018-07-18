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
 * @file   logevent.hpp
 *
 * @brief  [enter description of what this file contains]
 */

#pragma once

#include <string>
#include <gio/gio.h>

#include "log-helpers.hpp"


/**
 *  Basic Log Event container
 */
struct LogEvent
{
    /**
     *  Initializes an empty LogEvent struct.
     */
    LogEvent()
    {
        reset();
    }

    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     */
    LogEvent(const LogGroup grp, const LogCategory ctg,
             const std::string msg)
        : group(grp), category(ctg), message(msg)
    {
    }

    /**
     *  Initialize a LogEvent, based on a GVariant object containing
     *  a log entry.  See @Parse() for more information.
     *
     * @param status
     */
    LogEvent(GVariant *logev)
    {
        reset();
        Parse(logev);
    }


    /**
     *  Resets the LogEvent struct to a known and empty state
     */
    void reset()
    {
        group = LogGroup::UNDEFINED;
        category = LogCategory::UNDEFINED;
        message = "";
    }


    /**
     *  Checks if the LogEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty()
    {
        return (LogGroup::UNDEFINED == group)
               && (LogCategory::UNDEFINED == category)
               && message.empty();
    }

    /**
     *  Parses a GVariant object containing a Log signal.  The input
     *  GVariant needs to be of 'a{sv}' which is a named dictonary.  It
     *  must contain the following key values to be valid:
     *
     *     - (u) log_group       Translated into LogGroup
     *     - (u) log_category    Translated into LogCategory
     *     - (s) log_message     A string with the log message
     *
     * @param logevent  Pointer to the GVariant object containig the
     *                  log event
     */
    void Parse(GVariant *logevent)
    {
        GVariant *d = nullptr;
        unsigned int v = 0;

        d = g_variant_lookup_value(logevent, "log_group", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        if (v > 0 && v < LogGroup_str.size())
        {
            group = (LogGroup) v;
        }
        g_variant_unref(d);

        d = g_variant_lookup_value(logevent, "log_category", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        if (v > 0 && v < LogCategory_str.size())
        {
            category = (LogCategory) v;
        }
        g_variant_unref(d);

        gsize len;
        d = g_variant_lookup_value(logevent,
                                   "log_message", G_VARIANT_TYPE_STRING);
        message = std::string(g_variant_get_string(d, &len));
        g_variant_unref(d);
        if (len != message.size())
        {
            THROW_LOGEXCEPTION("Failed retrieving log event message text"
                               " (inconsistent length)");
        }
    }


    /**
     *  Makes it possible to write LogEvents in a readable format
     *  via iostreams, such as 'std::cout << event', where event is a
     *  LogEvent object.
     *
     * @param os  std::ostream where to write the data
     * @param ev  LogEvent to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded LogEvent information
     */
    friend std::ostream& operator<<(std::ostream& os , const LogEvent& ev)
    {
        return os << LogPrefix(ev.group, ev.category) << ev.message;
    }

    LogGroup group;
    LogCategory category;
    std::string message;
};
