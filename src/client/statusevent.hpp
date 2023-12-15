//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   backendstatus.hpp
 *
 * @brief  Declares the StatusEvent object, which includes a D-Bus GVariant
 *         value parser
 */

#pragma once

#include <cstdint>
#include <sstream>
#include <glib.h>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

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

    static DBus::Signals::SignalArgList SignalDeclaration() noexcept
    {
        return {{"code_major", glib2::DataType::DBus<StatusMajor>()},
                {"code_minor", glib2::DataType::DBus<StatusMinor>()},
                {"message", glib2::DataType::DBus<std::string>()}};
    }

    StatusEvent(const StatusMajor maj, const StatusMinor min, const std::string &msg = "")
    {
        reset();
        major = maj;
        minor = min;
        message = msg;
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
                throw DBus::Exception("StatusEvent", "Invalid status data");
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
        print_mode = static_cast<uint8_t>(PrintMode::ALL);
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
        print_mode = (unsigned short)m;
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
        return g_variant_new("(uus)",
                             static_cast<guint32>(major),
                             static_cast<guint32>(minor),
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
        GVariantBuilder *b = glib2::Builder::Create("a{sv}");
        g_variant_builder_add(b, "{sv}", "major", glib2::Value::Create(major));
        g_variant_builder_add(b, "{sv}", "minor", glib2::Value::Create(minor));
        g_variant_builder_add(b, "{sv}", "status_message", glib2::Value::Create(message));
        return glib2::Builder::Finish(b);
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
    friend std::ostream &operator<<(std::ostream &os, const StatusEvent &s)
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
            if (s.print_mode & (uint8_t)StatusEvent::PrintMode::MAJOR)
            {
                status_num << std::to_string(static_cast<unsigned>(s.major));
                status_str << StatusMajor_str[static_cast<unsigned>(s.major)];
            }
            if (s.print_mode == (uint8_t)StatusEvent::PrintMode::ALL)
            {
                status_num << ",";
                status_str << ", ";
            }
            if (s.print_mode & (uint8_t)StatusEvent::PrintMode::MINOR)
            {
                status_num << std::to_string(static_cast<unsigned>(s.minor));
                status_str << StatusMinor_str[static_cast<unsigned>(s.minor)];
            }
            status_num << "] ";

            return os << (s.show_numeric_status ? status_num.str() : "")
                      << status_str.str()
                      << (s.print_mode != (uint8_t)StatusEvent::PrintMode::NONE
                                  && !s.message.empty()
                              ? ": "
                              : "")
                      << (!s.message.empty() ? s.message : "");
        }
    }


    bool operator==(const StatusEvent &compare) const
    {
        return ((compare.major == (const StatusMajor)major)
                && (compare.minor == (const StatusMinor)minor)
                && (0 == compare.message.compare(message)));
    }

    bool operator!=(const StatusEvent &compare) const
    {
        return !(this->operator==(compare));
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
        reset();
        major = glib2::Value::Dict::Lookup<StatusMajor>(status, "major");
        minor = glib2::Value::Dict::Lookup<StatusMinor>(status, "minor");
        message = glib2::Value::Dict::Lookup<std::string>(status, "status_message");
    }


    /**
     *   Parses a GVvariant (uus) tupple containing the status object
     */
    void parse_tuple(GVariant *status)
    {
        reset();
        major = glib2::Value::Extract<StatusMajor>(status, 0);
        minor = glib2::Value::Extract<StatusMinor>(status, 1);
        message = glib2::Value::Extract<std::string>(status, 2);
    }
};
