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


namespace Events {

/**
 * Carries a status event record as reported by a VPN backend client
 */
struct Status
{
    enum class PrintMode : uint8_t
    {
        NONE = 0,
        MAJOR = 1,
        MINOR = 2,
        ALL = 3
    };

    StatusMajor major;
    StatusMinor minor;
    std::string message;
    uint8_t print_mode;
    bool show_numeric_status;


    Status(const StatusMajor maj, const StatusMinor min, const std::string &msg = "");
    Status();

    /**
     * Constructor which parses a GVariant containing either
     * a tuple (uus) or a dictionary containing a StatusEvent entry
     *
     * @param status  Status object (GVariant *) to parse
     */
    Status(GVariant *status);

    static DBus::Signals::SignalArgList SignalDeclaration() noexcept;


    /**
     *  Resets the current status to UNSET
     */
    void reset();

    /**
     *  Checks if the StatusEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty() const;

    /**
     *  Configures what should be printed
     * @param m
     */
    void SetPrintMode(const Status::PrintMode m);

    /**
     *  Compares the saved status against a specific state
     *
     * @param maj  StatusMajor to compare against the saved state
     * @param min  StatisMinro to compare
     *
     * @return Returns true if both maj and min matches the saved
     *         state
     */
    bool Check(const StatusMajor maj, const StatusMinor min) const;

    /**
     *  Create a D-Bus compliant GVariant object with the status information
     *  packed as a '(uus)' tuple
     *
     * @return Returns a pointer to a new GVariant (@g_variant_new())
     *         allocated object.
     */
    GVariant *GetGVariantTuple() const;

    /**
     *  Create a D-Bus compliant GVariant object with the status information
     *  packed as a key/value based dictionary.
     *
     * @return Returns a pointer to a new GVariant (@g_variant_new())
     *         allocated object.
     */
    GVariant *GetGVariantDict() const;

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
    friend std::ostream &operator<<(std::ostream &os, const Status &s)
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
            if (s.print_mode & static_cast<uint8_t>(Status::PrintMode::MAJOR))
            {
                status_num << std::to_string(static_cast<unsigned>(s.major));
                status_str << StatusMajor_str[static_cast<unsigned>(s.major)];
            }
            if (s.print_mode == static_cast<uint8_t>(Status::PrintMode::ALL))
            {
                status_num << ",";
                status_str << ", ";
            }
            if (s.print_mode & static_cast<uint8_t>(Status::PrintMode::MINOR))
            {
                status_num << std::to_string(static_cast<unsigned>(s.minor));
                status_str << StatusMinor_str[static_cast<unsigned>(s.minor)];
            }
            status_num << "] ";

            return os << (s.show_numeric_status ? status_num.str() : "")
                      << status_str.str()
                      << (s.print_mode != static_cast<uint8_t>(Status::PrintMode::NONE)
                                  && !s.message.empty()
                              ? ": "
                              : "")
                      << (!s.message.empty() ? s.message : "");
        }
    }


    bool operator==(const Status &compare) const;
    bool operator!=(const Status &compare) const;


  private:
    /**
     *   Parses a GVvariant dictionary containing the status object
     */
    void parse_dict(GVariant *status);

    /**
     *   Parses a GVvariant (uus) tupple containing the status object
     */
    void parse_tuple(GVariant *status);
};

} // namespace Events
