//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2021  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2021  David Sommerseth <davids@openvpn.net>
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
    enum class PrintMode : uint8_t
    {
        NONE = 0,
        MAJOR = 1,
        MINOR = 2,
        ALL = 3
    };

    StatusEvent(const StatusMajor maj, const StatusMinor min,
                const std::string& msg)
    {
        reset();
        major = maj;
        minor = min;
        message = msg;
    }


    StatusEvent(const StatusMajor& maj, const StatusMinor& min)
    {
        reset();
        major = maj;
        minor = min;
    }


    StatusEvent()
    {
        reset();
    }


    /**
     * Constructor which parses a GVariant containing either
     * a tuple (uus) or a dictionary containing a StatusEvent entry
     *
     * @param status  Status object (GVariant *) to parse
     */
    StatusEvent(GVariant *status)
    {
        reset();
        if (nullptr != status)
        {
            std::string g_type(g_variant_get_type_string(status));
            if ("(uus)" == g_type)
            {
                parse_tuple(status);
            }
            else if ("a{sv}" == g_type)
            {
                parse_dict(status);
            }
            else
            {
                THROW_DBUSEXCEPTION("StatusEvent", "Invalid status data");
            }
        }
    }


    /**
     *  Resets the current status to UNSET
     */
    void reset()
    {
        major = StatusMajor::UNSET;
        minor = StatusMinor::UNSET;
        message.clear();
        print_mode = (uint8_t) PrintMode::ALL;
#ifdef DEBUG_CORE_EVENTS
        show_numeric_status = true;
#else
        show_numeric_status = false;
#endif
    }


    /**
     *  Checks if the StatusEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty() const
    {
        return (StatusMajor::UNSET == major)
               && (StatusMinor::UNSET == minor)
               && message.empty();
    }


    /**
     *  Configures what should be printed
     * @param m
     */
    void PrintMode(const StatusEvent::PrintMode m)
    {
        print_mode = (unsigned short) m;
    }


    /**
     *  Compares the saved status against a specific state
     *
     * @param maj  StatusMajor to compare against the saved state
     * @param min  StatisMinro to compare
     *
     * @return Returns true if both maj and min matches the saved
     *         state
     */
    bool Check(const StatusMajor maj, const StatusMinor min) const
    {
        return (maj == major) && (min == minor);
    }


    /**
     *  Create a D-Bus compliant GVariant object with the status information
     *  packed as a '(uus)' tuple
     *
     * @return Returns a pointer to a new GVariant (@g_variant_new())
     *         allocated object.
     */
    GVariant *GetGVariantTuple() const
    {
        return g_variant_new("(uus)", (guint32) major, (guint32) minor,
                             message.c_str());
    }


    /**
     *  Create a D-Bus compliant GVariant object with the status information
     *  packed as a key/value based dictionary.
     *
     * @return Returns a pointer to a new GVariant (@g_variant_new())
     *         allocated object.
     */
    GVariant *GetGVariantDict() const
    {
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (b, "{sv}", "major",
                               g_variant_new_uint32((guint) major));
        g_variant_builder_add (b, "{sv}", "minor",
                               g_variant_new_uint32((guint) minor));
        g_variant_builder_add (b, "{sv}", "status_message",
                               g_variant_new_string(message.c_str()));
        GVariant *data= g_variant_builder_end(b);
        g_variant_builder_unref(b);
        return data;
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
            std::stringstream status_num;
            std::stringstream status_str;

            status_num << "[";
            if (s.print_mode & (uint8_t) StatusEvent::PrintMode::MAJOR)
            {
                status_num << std::to_string((unsigned) s.major);
                status_str << StatusMajor_str[(unsigned) s.major];
            }
            if (s.print_mode == (uint8_t) StatusEvent::PrintMode::ALL)
            {
                status_num << ",";
                status_str << ", ";
            }
            if (s.print_mode & (uint8_t) StatusEvent::PrintMode::MINOR)
            {
                status_num << std::to_string((unsigned) s.minor);
                status_str << StatusMinor_str[(unsigned) s.minor];
            }
            status_num << "] ";

            return os << (s.show_numeric_status ? status_num.str() : "")
                      << status_str.str()
                      << (s.print_mode != (uint8_t) StatusEvent::PrintMode::NONE
                          && !s.message.empty() ? ": " : "")
                      << (!s.message.empty() ? s.message : "");
        }
    }


    bool operator==(const StatusEvent& compare) const
    {
        return ((compare.major == (const StatusMajor) major)
               && (compare.minor == (const StatusMinor) minor)
               && (0 == compare.message.compare(message)));
    }


    bool operator!=(const StatusEvent& compare) const
    {
        return !(this->operator ==(compare));
    }

    StatusMajor major;
    StatusMinor minor;
    std::string message;
    uint8_t print_mode;
    bool show_numeric_status;

private:
    /**
     *   Parses a GVvariant dictionary containing the status object
     */
    void parse_dict(GVariant *status)
    {
        GVariant *d = nullptr;
        unsigned int v = 0;

        // FIXME: Should type-check better that the input GVariant
        //        contains the proper fields for a status object
        reset();
        d = g_variant_lookup_value(status, "major", G_VARIANT_TYPE_UINT32);
        if (!d)
        {
            THROW_DBUSEXCEPTION("StatusEvent", "Incorrect StatusEvent dict "
                                "(missing 'major')");
        }
        v = g_variant_get_uint32(d);
        major = (StatusMajor) v;
        g_variant_unref(d);

        d = g_variant_lookup_value(status, "minor", G_VARIANT_TYPE_UINT32);
        if (!d)
        {
            THROW_DBUSEXCEPTION("StatusEvent", "Incorrect StatusEvent dict "
                                "(missing 'minor')");
        }
        v = g_variant_get_uint32(d);
        minor = (StatusMinor) v;
        g_variant_unref(d);

        gsize len;
        d = g_variant_lookup_value(status,
                                   "status_message", G_VARIANT_TYPE_STRING);
        if (!d)
        {
            THROW_DBUSEXCEPTION("StatusEvent", "Incorrect StatusEvent dict "
                                "(missing 'status_message')");
        }
        message = std::string(g_variant_get_string(d, &len));
        g_variant_unref(d);
        if (len != message.size())
        {
            THROW_DBUSEXCEPTION("StatusEvent",
                                "Failed retrieving status message text "
                                "(inconsistent length)");
        }
    }


    /**
     *   Parses a GVvariant (uus) tupple containing the status object
    */
    void parse_tuple(GVariant *status)
    {
        guint maj = 0;
        guint min = 0;
        gchar *msg = nullptr;
        g_variant_get(status, "(uus)", &maj, &min, &msg);

        reset();
        major = (StatusMajor) maj;
        minor = (StatusMinor) min;
        if (msg)
        {
            message = std::string(msg);
            g_free(msg);
        }
    }
};
