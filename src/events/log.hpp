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
#include "log/logtag.hpp"


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
     * @param filter_nl  (optional) Filter out newline (\n) characters in log message (default true)
     */
    Log(const LogGroup grp,
        const LogCategory ctg,
        const std::string &msg,
        bool filter_nl = true);

    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     * @param filter_nl  (optional) Filter out newline (\n) characters in log message (default true)
     */
    Log(const LogGroup grp,
        const LogCategory ctg,
        const std::string &session_token,
        const std::string &msg,
        bool filter_nl = true);

    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp            LogGroup value to use.
     * @param ctg            LogCategory value to use.
     * @param session_token  char * containing the log message to use.
     * @param msg            char * containing the log message to use.
     * @param filter_nl      (optional) Filter out newline (\n) characters in log message (default true)
     */
    Log(const LogGroup grp,
        const LogCategory ctg,
        const char *session_token,
        const char *msg,
        bool filter_nl = true);

    Log(const Log &logev, const std::string &session_token);

    /**
     *  Remove the session token from the current log event
     *
     *  This results in the Log::str() and operator<<() functions not
     *  appending the session token to the string output.
     */
    void RemoveToken();

    /**
     *  Attach meta information about the D-Bus sender of the log event.
     *  This can only be called once.  Additional calls will be silently
     *  ignored.
     *
     *  @param sender  DBus::Signals::Target object containing the sender details
     */
    void SetDBusSender(DBus::Signals::Target::Ptr sender);

    /**
     *  Adds a LogTag which will prefix the log message
     *
     * @param tag  LogTag object to use
     */
    void AddLogTag(LogTag::Ptr tag) noexcept;

    /**
     *  Retrieve the currently set LogTag object
     *
     * @return LogTag::Ptr of the LogTag object; nullptr if not set
     */
    LogTag::Ptr GetLogTag() const noexcept;

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
    const std::string str(unsigned short indent = 0, bool prefix = true) const;

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
    LogTag::Ptr logtag = nullptr;
    Format format = Format::AUTO;


  private:
    /**
     *  Internal helper function, removes NL characters at the end of the
     *  log event message string
     *  Internal helper function, filtering out unwanted characters
     *  from log messages.  It will also remove any trailing newline
     *  characters.
     *
     *  If filter_nl is false, it will remove any char values < 0x20.
     *  If filter_nl is true, it will only allow \n below 0x20.
     *
     *  @param filter_nl  Filter out newline (\n) characters in log message
     */
    void filter_log_message(bool filter_nl);
};


/**
 *  Helper function to generate a Log event where the group and category
 *  fields are passed as strings and parsed into LogGroup and LogCategory
 *  types.
 *
 * @param grp_s       std::string containing the LogGroup string representation
 * @param ctg_s       std::string containing the LogCategory string representation
 * @param sess_token  std::string containing the session token value
 * @param msg         std::string containing the log message
 * @param filter_nl  (optional) Filter out newline (\n) characters in log message (default true)
 */
[[nodiscard]] Log ParseLog(const std::string &grp_s,
                           const std::string &ctg_s,
                           const std::string &sess_token,
                           const std::string &msg,
                           bool filter_nl = true);


/**
 *  Helper function to generate a Log event where the group and category
 *  fields are passed as strings and parsed into LogGroup and LogCategory
 *  types.
 *
 * @param grp_s  std::string containing the LogGroup string representation
 * @param ctg_s  std::string containing the LogCategory string representation
 * @param msg    std::string containing the log message
 * @param filter_nl  (optional) Filter out newline (\n) characters in log message (default true)
 */
[[nodiscard]] Log ParseLog(const std::string &grp_s,
                           const std::string &ctg_s,
                           const std::string &msg,
                           bool filter_nl = true);


/**
 *  Helper function parsing a GVariant object containing
 *  a Log signal into an Event::Log object.  It supports both tuple based and
 *  dictionary based Log signals. See @parse_dict() and @parse_tuple for more
 *  information.
 *
 * @param logev  Pointer to a GVariant object containing the the Log event
 * @param sender (optional) DBus::Signals::Target object with details about
 *               the signal sender
 * @throws LogException on invalid input data
 */
[[nodiscard]] Log ParseLog(GVariant *logev, DBus::Signals::Target::Ptr sender = nullptr);


} // namespace Events
