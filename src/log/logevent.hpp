//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logevent.hpp
 *
 * @brief  [enter description of what this file contains]
 */

#pragma once

#include <algorithm>
#include <iomanip>
#include <string>
#include <gio/gio.h>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "log-helpers.hpp"


/**
 *  Basic Log Event container
 */
struct LogEvent
{
    enum class Format : uint8_t
    {
        AUTO,
        NORMAL,
        SESSION_TOKEN
    };

    static const DBus::Signals::SignalArgList SignalDeclaration(bool with_session_token = false) noexcept
    {
        if (with_session_token)
        {
            return {{"group", glib2::DataType::DBus<LogGroup>()},
                    {"level", glib2::DataType::DBus<LogCategory>()},
                    {"session_token", glib2::DataType::DBus<std::string>()},
                    {"message", glib2::DataType::DBus<std::string>()}};
        }
        else
        {
            return {{"group", glib2::DataType::DBus<LogGroup>()},
                    {"level", glib2::DataType::DBus<LogCategory>()},
                    {"message", glib2::DataType::DBus<std::string>()}};
        }
    }


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
    LogEvent(const LogGroup grp,
             const LogCategory ctg,
             const std::string &msg)
        : group(grp), category(ctg), message(msg)
    {
        remove_trailing_nl();
        format = Format::NORMAL;
    }


    /**
     *  Initialize the LogEvent object with the provided details.
     *
     * @param grp  LogGroup value to use.
     * @param ctg  LogCategory value to use.
     * @param msg  std::string containing the log message to use.
     */
    LogEvent(const LogGroup grp,
             const LogCategory ctg,
             const std::string &session_token,
             const std::string &msg)
        : group(grp), category(ctg),
          session_token(session_token), message(msg)
    {
        remove_trailing_nl();
        format = Format::SESSION_TOKEN;
    }

    LogEvent(const LogEvent &logev, const std::string &session_token)
        : group(logev.group), category(logev.category),
          session_token(session_token), message(logev.message)
    {
        remove_trailing_nl();
        format = Format::SESSION_TOKEN;
    }


    LogEvent(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &msg)
        : message(msg)
    {
        parse_group_category(grp_s, ctg_s);
        remove_trailing_nl();
        format = Format::NORMAL;
    }


    LogEvent(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &sess_token,
             const std::string &msg)
        : message(msg)
    {
        parse_group_category(grp_s, ctg_s);
        remove_trailing_nl();
        if (sess_token.empty())
        {
            format = Format::NORMAL;
        }
        else
        {
            session_token = sess_token;
            format = Format::SESSION_TOKEN;
        }
    }


    /**
     *  Initialize a LogEvent, based on a GVariant object containing
     *  a Log signal.  It supports both tuple based and dictonary based
     *  Log signals. See @parse_dict() and @parse_tuple for more
     *  information.
     *
     * @param logev  Pointer to a GVariant object containing the the Log event
     * @throws LogException on invalid input data
     */
    LogEvent(GVariant *logev)
    {
        reset();
        if (nullptr != logev)
        {
            std::string g_type(g_variant_get_type_string(logev));
            if ("a{sv}" == g_type)
            {
                parse_dict(logev);
            }
            else if ("(uus)" == g_type)
            {
                parse_tuple(logev, false);
                format = Format::NORMAL;
            }
            else if ("(uuss)" == g_type)
            {
                parse_tuple(logev, true);
                format = Format::SESSION_TOKEN;
            }
            else
            {
                throw LogException("LogEvent: Invalid LogEvent data type");
            }
            remove_trailing_nl();
        }
    }


    void RemoveToken()
    {
        format = Format::NORMAL;
        session_token.clear();
    }

    /**
     *  Create a GVariant object containing a tuple formatted object for
     *  a Log signal of the current LogEvent.
     *
     * @return  Returns a pointer to a GVariant object with the formatted
     *          data
     */
    GVariant *GetGVariantTuple() const
    {
        if (Format::SESSION_TOKEN == format
            || (Format::AUTO == format && !session_token.empty()))
        {
            return g_variant_new("(uuss)",
                                 static_cast<uint32_t>(group),
                                 static_cast<uint32_t>(category),
                                 session_token.c_str(),
                                 message.c_str());
        }
        else
        {
            return g_variant_new("(uus)",
                                 static_cast<uint32_t>(group),
                                 static_cast<uint32_t>(category),
                                 message.c_str());
        }
    }


    /**
     *  Create a GVariant object containing a dictionary formatted object
     *  for a Log a signal of the current LogEvent.
     *
     * @return  Returns a pointer to a GVariant object with the formatted
     *          data
     */
    GVariant *GetGVariantDict() const
    {
        GVariantBuilder *b = glib2::Builder::Create("a{sv}");
        g_variant_builder_add(b,"{sv}", "log_group", glib2::Value::Create(group));
        g_variant_builder_add(b, "{sv}", "log_category", glib2::Value::Create(category));
        if (!session_token.empty() || Format::SESSION_TOKEN == format)
        {
            g_variant_builder_add(b, "{sv}", "log_session_token", glib2::Value::Create(session_token));
        }
        g_variant_builder_add(b, "{sv}", "log_message", glib2::Value::Create(message));
        return glib2::Builder::Finish(b);
    }


    /**
     *  Retrieve a string describing the Log Group of this LogEvent
     *
     * @return std::string with the log group description
     */
    const std::string GetLogGroupStr() const
    {
        if ((uint8_t)group >= LogGroupCount)
        {
            return std::string("[group:"
                               + std::to_string(static_cast<uint8_t>(group))
                               + "]");
        }
        return LogGroup_str[(uint8_t)group];
    }


    /**
     *  Retrieve a string describing the Log Category of this LogEvent
     *
     * @return std::string with the log category description
     */
    const std::string GetLogCategoryStr() const
    {
        if ((uint8_t)category > 8)
        {
            return std::string("[category:" + std::to_string((uint8_t)category) + "]");
        }
        return LogCategory_str[(uint8_t)category];
    }


    /**
     *  Resets the LogEvent struct to a known and empty state
     */
    void reset()
    {
        group = LogGroup::UNDEFINED;
        category = LogCategory::UNDEFINED;
        session_token.clear();
        message.clear();
        format = Format::AUTO;
    }


    /**
     *  Checks if the LogEvent object is empty
     *
     * @return Returns true if it is empty/unused
     */
    bool empty() const
    {
        return (LogGroup::UNDEFINED == group)
               && (LogCategory::UNDEFINED == category)
               && session_token.empty()
               && message.empty();
    }


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
    const std::string str(unsigned short indent, bool prefix = true) const
    {
        std::ostringstream r;
        if (prefix)
        {
            r << LogPrefix(group, category);
        }
        if (indent > 0)
        {
            std::stringstream msg;
            std::string line;
            msg << message;
            bool first = true;
            while (std::getline(msg, line, '\n'))
            {
                if (!first)
                {
                    r << std::endl
                      << std::setw(indent) << std::setfill(' ') << " ";
                }
                first = false;
                r << line;
            }
        }
        else
        {
            r << message;
        }

        return std::string(r.str());
    }


    /**
     *  Simple wrapper of the @str(unsigned short, bool) method.
     *  This  will consider the indent level set via the indent_nl
     *  member variable.
     *
     * @return const std::string  Returs a formatted string of the log event
     */
    const std::string str() const
    {
        return str(indent_nl, true);
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
    friend std::ostream &operator<<(std::ostream &os, const LogEvent &ev)
    {
        return os << ev.str();
    }


    bool operator==(const LogEvent &compare) const
    {
        if (session_token.empty())
        {
            return ((compare.group == group)
                    && (compare.category == category)
                    && (0 == compare.message.compare(message)));
        }
        else
        {
            return ((compare.group == group)
                    && (compare.category == category)
                    && (0 == compare.session_token.compare(session_token))
                    && (0 == compare.message.compare(message)));
        }
    }


    bool operator!=(const LogEvent &compare) const
    {
        return !(this->operator==(compare));
    }


    LogGroup group = LogGroup::UNDEFINED;
    LogCategory category = LogCategory::UNDEFINED;
    std::string session_token = {};
    std::string message = {};
    Format format = Format::AUTO;
    unsigned short indent_nl = 0;


  private:
    void remove_trailing_nl()
    {
        message.erase(message.find_last_not_of("\n") + 1);
    }


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
    void parse_dict(GVariant *logevent)
    {
        group = glib2::Value::Dict::Lookup<LogGroup>(logevent, "log_group");
        category = glib2::Value::Dict::Lookup<LogCategory>(logevent, "log_category");
        try
        {
            session_token = glib2::Value::Dict::Lookup<std::string>(logevent,
                                                                    "log_session_token");
            format = Format::SESSION_TOKEN;
        }
        catch (const glib2::Utils::Exception &)
        {
            // This is fine; the log_session_token may not be available
            // and then we treat this event as a "normal" LogEvent without
            // the sessoin token value
            format = Format::NORMAL;
        }
        message = glib2::Value::Dict::Lookup<std::string>(logevent,
                                                          "log_message");
    }


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
    void parse_tuple(GVariant *logevent, bool with_session_token)
    {
        if (!with_session_token)
        {
            glib2::Utils::checkParams(__func__, logevent, "(uus)", 3);
            group = glib2::Value::Extract<LogGroup>(logevent, 0);
            category = glib2::Value::Extract<LogCategory>(logevent, 1);
            message = glib2::Value::Extract<std::string>(logevent, 2);
        }
        else
        {
            glib2::Utils::checkParams(__func__, logevent, "(uuss)", 4);
            group = glib2::Value::Extract<LogGroup>(logevent, 0);
            category = glib2::Value::Extract<LogCategory>(logevent, 1);
            session_token = glib2::Value::Extract<std::string>(logevent, 2);
            message = glib2::Value::Extract<std::string>(logevent, 3);
        }
    }

    /**
     *  Parses group and category strings to the appropriate LogGroup
     *  and LogGroup enum values.  This method sets the group and category members
     *  directly and does not return any values.  String values not found will
     *  result in the UNDEFINED value being set.
     *
     * @param grp_s std::string containing the human readable LogGroup string
     * @param ctg_s std::string containing the human readable LogCategory string
     */
    void parse_group_category(const std::string &grp_s, const std::string &ctg_s)
    {
        auto grp_item = std::find(LogGroup_str.begin(), LogGroup_str.end(), grp_s);
        group = (grp_item != LogGroup_str.end()
                     ? static_cast<LogGroup>(std::distance(LogGroup_str.begin(), grp_item))
                     : LogGroup::UNDEFINED);
        auto catg_item = std::find(LogCategory_str.begin(), LogCategory_str.end(), ctg_s);
        category = (catg_item != LogCategory_str.end()
                        ? static_cast<LogCategory>(std::distance(LogCategory_str.begin(), catg_item))
                        : LogCategory::UNDEFINED);
    }
};
