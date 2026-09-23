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
#include <fmt/ostream.h>
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

    static DBus::Signals::SignalArgList SignalDeclaration(bool with_session_token = false) noexcept;

    /**
     *  Initializes an empty LogEvent struct.
     */
    Log();

    /**
     *  Initialize the LogEvent object with the provided details.
     *  The message is stored as-is; use KeepNL() to preserve newlines
     *  in str() output, otherwise newlines are filtered.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     */
    Log(LogGroup grp,
        LogCategory ctg,
        const std::string &msg);

    /**
     *  Initialize the LogEvent object with session token and message.
     *  The message is stored as-is; use KeepNL() to preserve newlines
     *  in str() output, otherwise newlines are filtered.
     *
     * @param grp            LogGroup value to use.
     * @param ctg            LogCategory value to use.
     * @param session_token  std::string containing the session token.
     * @param msg            std::string containing the log message to use.
     */
    Log(LogGroup grp,
        LogCategory ctg,
        const std::string &session_token,
        const std::string &msg);

    /**
     *  Initialize the LogEvent object with session token and message.
     *  The message is stored as-is; use KeepNL() to preserve newlines
     *  in str() output, otherwise newlines are filtered.
     *
     * @param grp            LogGroup value to use.
     * @param ctg            LogCategory value to use.
     * @param session_token  char * containing the session token.
     * @param msg            char * containing the log message to use.
     */
    Log(LogGroup grp,
        LogCategory ctg,
        const char *session_token,
        const char *msg);

    Log(const Log &logev, const std::string &session_token);

    /**
     *  Preserve newline characters when str() formats the log message.
     *  By default str() filters out newline characters.
     *
     * @return Reference to this object, for call chaining.
     */
    Log &KeepNL() noexcept;

    /**
     *  Retrieve the log message
     *
     *  This is the plain message input filtered for various
     *  control characters.  Newline handling depends
     *  if Events::Log::KeepNL() has been called.
     *
     * @return std::string
     */
    std::string GetMessage() const;

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
     *
     * @return Log
     */
    Log &AddLogTag(LogTag::Ptr tag) noexcept;

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
    std::string GetLogGroupStr() const;

    /**
     *  Retrieve a string describing the Log Category of this LogEvent
     *
     * @return std::string with the log category description
     */
    std::string GetLogCategoryStr() const;

    /**
     *  Resets the LogEvent struct to a known and empty state
     */
    void reset();

    /**
     *  Checks if the Events::Log object is empty
     *
     * @param only_message (optional) If true, only consider the
     *                     message field and ignore log category/group
     *                     values
     * @return Returns true if it is empty/unused
     */
    bool empty(bool only_message = false) const;


    Log &SetIndent(uint8_t spaces);

    /**
     *  Extract a formatted std::string of the log event.
     *
     *  The optional prefix argument enables or disables the
     *  log group and category string added before the log
     *  message.  The default is to enable this prefix.
     *
     * @param prefix              bool enabling/disabling log group/category prefix
     * @return const std::string  Returns a formatted string of the log event.
     */
    std::string str(bool prefix = true) const;

    bool operator==(const Log &compare) const;
    bool operator!=(const Log &compare) const;


    /**
     *  std::string operator()
     *
     *  Retrieve a human readable string when this object is treated as
     *  a std::string
     *
     * @return std::string
     */
    operator std::string() const;

    /**
     *  Makes it possible to write Events::Log in a readable format
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
    DBus::Signals::Target::Ptr sender = nullptr;
    LogTag::Ptr logtag = nullptr;
    Format format = Format::AUTO;

  private:
    bool keep_nl_ = false;
    uint8_t indent_ = 0;
    std::string message_;
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
 */
[[nodiscard]] Log ParseLog(const std::string &grp_s,
                           const std::string &ctg_s,
                           const std::string &sess_token,
                           const std::string &msg);


/**
 *  Helper function to generate a Log event where the group and category
 *  fields are passed as strings and parsed into LogGroup and LogCategory
 *  types.
 *
 * @param grp_s  std::string containing the LogGroup string representation
 * @param ctg_s  std::string containing the LogCategory string representation
 * @param msg    std::string containing the log message
 */
[[nodiscard]] Log ParseLog(const std::string &grp_s,
                           const std::string &ctg_s,
                           const std::string &msg);


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


/**
 *  libfmt / fmt::format support, wrapping
 *  Events::Status::operator<<()
 */
template <>
struct fmt::formatter<Events::Log> : fmt::ostream_formatter
{
};