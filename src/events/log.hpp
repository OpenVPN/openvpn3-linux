//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   events/log.hpp
 *
 * @brief  [enter description of what this file contains]
 */

#pragma once

#include <algorithm>
#include <iomanip>
#include <string>
#include <gdbuspp/signals/group.hpp>

#include "log/log-helpers.hpp"


namespace Events {

/**
 *  Basic Log Event container
 */
struct Log
{
    enum class Format : uint8_t
    {
        AUTO,
        NORMAL,
        SESSION_TOKEN
    };

    static const DBus::Signals::SignalArgList SignalDeclaration(bool with_session_token = false) noexcept;

    /**
     *  Initializes an empty LogEvent struct.
     */
    Log();

    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     */
    Log(const LogGroup grp,
        const LogCategory ctg,
        const std::string &msg);

    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     */
    Log(const LogGroup grp,
        const LogCategory ctg,
        const std::string &session_token,
        const std::string &msg);

    Log(const Log &logev, const std::string &session_token);

    Log(const std::string &grp_s,
        const std::string &ctg_s,
        const std::string &msg);

    Log(const std::string &grp_s,
        const std::string &ctg_s,
        const std::string &sess_token,
        const std::string &msg);

    /**
     *  Initialize a LogEvent, based on a GVariant object containing
     *  a Log signal.  It supports both tuple based and dictonary based
     *  Log signals. See @parse_dict() and @parse_tuple for more
     *  information.
     *
     * @param logev  Pointer to a GVariant object containing the the Log event
     * @param sender (optional) DBus::Signals::Target object with details about
     *               the signal sender
     * @throws LogException on invalid input data
     */
    Log(GVariant *logev, DBus::Signals::Target::Ptr sender = nullptr);


    /**
     *  Remove the session token from the current log event
     *
     *  This results in the Log::str() and operator<<() functions not
     *  appending the session token to the string output.
     */
    void RemoveToken();

    /**
     *  Create a GVariant object containing a tuple formatted object for
     *  a Log signal of the current LogEvent.
     *
     * @return  Returns a pointer to a GVariant object with the formatted
     *          data
     */
    GVariant *GetGVariantTuple() const;

    /**
     *  Create a GVariant object containing a dictionary formatted object
     *  for a Log a signal of the current LogEvent.
     *
     * @return  Returns a pointer to a GVariant object with the formatted
     *          data
     */
    GVariant *GetGVariantDict() const;

    /**
     *  Retrieve a string describing the Log Group of this LogEvent
     *
     * @return std::string with the log group description
     */
    const std::string GetLogGroupStr() const;

    /**
     *  Retrieve a string describing the Log Category of this LogEvent
     *
     * @return std::string with the log category description
     */
    const std::string GetLogCategoryStr() const;

    /**
     *  Resets the LogEvent struct to a known and empty state
     */
    void reset();

    /**
     *  Checks if the LogEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty() const;

    /**
     *  Extract a formatted std::string of the log event.
     *
     *  The indent argument controls if indent width to use
     *  when a log message contains multiple lines.  The
     *  following lines will be indented with this width.
     *
     *  The optional prefix argument enables or disables the
     *  log group and category string added before the log
     *  message.  The default is to enable this prefix.
     *
     * @param indent              unsigned short of the number of spaces to
     *                            use for indenting
     * @param prefix              bool enabling/disabling log group/category prefix
     * @return const std::string  Returns a formatted string of the log event.
     */
    const std::string str(unsigned short indent, bool prefix = true) const;

    /**
     *  Simple wrapper of the @str(unsigned short, bool) method.
     *  This  will consider the indent level set via the indent_nl
     *  member variable.
     *
     * @return const std::string  Returs a formatted string of the log event
     */
    const std::string str() const;

    bool operator==(const Log &compare) const;
    bool operator!=(const Log &compare) const;

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
    friend std::ostream &operator<<(std::ostream &os, const Log &ev)
    {
        return os << ev.str();
    }

    LogGroup group = LogGroup::UNDEFINED;
    LogCategory category = LogCategory::UNDEFINED;
    std::string session_token = {};
    std::string message = {};
    DBus::Signals::Target::Ptr sender = nullptr;
    Format format = Format::AUTO;
    unsigned short indent_nl = 0;


  private:
    /**
     *  Internal helper function, removes NL characters at the end of the
     *  log event message string
     */
    void remove_trailing_nl();

    /**
     *  Parses a GVariant object containing a Log signal.  The input
     *  GVariant needs to be of 'a{sv}' which is a named dictionary.  It
     *  must contain the following key values to be valid:
     *
     *     - (u) log_group          Translated into LogGroup
     *     - (u) log_category       Translated into LogCategory
     *     - (s) log_session_token  An optional session token string
     *     - (s) log_message        A string with the log message
     *
     * @param logevent  Pointer to the GVariant object containig the
     *                  log event
     */
    void parse_dict(GVariant *logevent);

    /**
     *  Parses a tuple oriented GVariant object matching the data type
     *  for a LogEvent object.  The data type must be (uus) if the
     *  GVariant object is does not carry a session token value; otherwise
     *  it must be (uuss) if it does.
     *
     * @param logevent            Pointer to the GVariant object containig the
     *                            log event
     * @param with_session_token  Boolean flag indicating if the logevent
     *                            GVariant object is expected to contain a
     *                            session token.
     */
    void parse_tuple(GVariant *logevent, bool with_session_token);

    /**
     *  Parses group and category strings to the appropriate LogGroup
     *  and LogGroup enum values.  This method sets the group and category members
     *  directly and does not return any values.  String values not found will
     *  result in the UNDEFINED value being set.
     *
     * @param grp_s std::string containing the human readable LogGroup string
     * @param ctg_s std::string containing the human readable LogCategory string
     */
    void parse_group_category(const std::string &grp_s, const std::string &ctg_s);
};

} // namespace Events
