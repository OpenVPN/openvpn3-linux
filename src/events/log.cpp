//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   events/log.cpp
 *
 * @brief  [enter description of what this file contains]
 */

#include <algorithm>
#include <iomanip>
#include <string>
#include <gio/gio.h>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "common/string-utils.hpp"
#include "log.hpp"


namespace Events {

const DBus::Signals::SignalArgList Log::SignalDeclaration(bool with_session_token) noexcept
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


Log::Log()
{
    reset();
}


Log::Log(const LogGroup grp,
         const LogCategory ctg,
         const std::string &msg,
         bool filter_nl)
    : group(grp), category(ctg),
      message(filter_ctrl_chars(msg, filter_nl))
{
    format = Format::NORMAL;
}


Log::Log(const LogGroup grp,
         const LogCategory ctg,
         const std::string &session_token,
         const std::string &msg,
         bool filter_nl)
    : group(grp), category(ctg),
      session_token(session_token),
      message(filter_ctrl_chars(msg, filter_nl))
{
    format = Format::SESSION_TOKEN;
}


Log::Log(const LogGroup grp,
         const LogCategory ctg,
         const char *session_token,
         const char *msg,
         bool filter_nl)
    : group(grp), category(ctg),
      session_token(session_token),
      message(filter_ctrl_chars(msg, filter_nl))
{
    format = Format::SESSION_TOKEN;
}


Log::Log(const Log &logev, const std::string &session_token)
    : group(logev.group), category(logev.category),
      session_token(session_token),
      message(filter_ctrl_chars(logev.message, false))
{
    format = Format::SESSION_TOKEN;
}


void Log::RemoveToken()
{
    format = Format::NORMAL;
    session_token.clear();
}


void Log::SetDBusSender(DBus::Signals::Target::Ptr sndr)
{
    if (!sender)
    {
        sender = sndr;
    }
}


void Log::AddLogTag(LogTag::Ptr tag) noexcept
{
    logtag = tag;
}


LogTag::Ptr Log::GetLogTag() const noexcept
{
    return logtag;
}


GVariant *Log::GetGVariantTuple() const
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


GVariant *Log::GetGVariantDict() const
{
    GVariantBuilder *b = glib2::Builder::Create("a{sv}");
    g_variant_builder_add(b, "{sv}", "log_group", glib2::Value::Create(group));
    g_variant_builder_add(b, "{sv}", "log_category", glib2::Value::Create(category));
    if (!session_token.empty() || Format::SESSION_TOKEN == format)
    {
        g_variant_builder_add(b, "{sv}", "log_session_token", glib2::Value::Create(session_token));
    }
    g_variant_builder_add(b, "{sv}", "log_message", glib2::Value::Create(message));
    return glib2::Builder::Finish(b);
}


const std::string Log::GetLogGroupStr() const
{
    if ((uint8_t)group >= LogGroupCount)
    {
        return std::string("[group:"
                           + std::to_string(static_cast<uint8_t>(group))
                           + "]");
    }
    return LogGroup_str[(uint8_t)group];
}


const std::string Log::GetLogCategoryStr() const
{
    if ((uint8_t)category > 8)
    {
        return std::string("[category:" + std::to_string((uint8_t)category) + "]");
    }
    return LogCategory_str[(uint8_t)category];
}


void Log::reset()
{
    group = LogGroup::UNDEFINED;
    category = LogCategory::UNDEFINED;
    session_token.clear();
    message.clear();
    format = Format::AUTO;
}


bool Log::empty() const
{
    return (LogGroup::UNDEFINED == group)
           && (LogCategory::UNDEFINED == category)
           && session_token.empty()
           && message.empty();
}


const std::string Log::str(unsigned short indent, bool prefix) const
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


bool Log::operator==(const Log &compare) const
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


bool Log::operator!=(const Log &compare) const
{
    return !(this->operator==(compare));
}


namespace {
/**
 *  Parses group and category strings to the appropriate LogGroup
 *  and LogGroup enum values.  This method sets the group and category members
 *  directly and does not return any values.  String values not found will
 *  result in the UNDEFINED value being set.
 *
 * @param grp_s std::string containing the human readable LogGroup string
 * @param ctg_s std::string containing the human readable LogCategory string
 */
static std::pair<LogGroup, LogCategory> parse_group_category(const std::string &grp_s, const std::string &ctg_s)
{
    auto grp_item = std::find(LogGroup_str.begin(), LogGroup_str.end(), grp_s);
    LogGroup group = (grp_item != LogGroup_str.end()
                          ? static_cast<LogGroup>(std::distance(LogGroup_str.begin(), grp_item))
                          : LogGroup::UNDEFINED);
    auto catg_item = std::find(LogCategory_str.begin(), LogCategory_str.end(), ctg_s);
    LogCategory category = (catg_item != LogCategory_str.end()
                                ? static_cast<LogCategory>(std::distance(LogCategory_str.begin(), catg_item))
                                : LogCategory::UNDEFINED);
    return std::make_pair(group, category);
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
 *
 * @return Events::Log object of the parsed GVariant object
 */
Log parse_dict(GVariant *logevent)
{
    auto group = glib2::Value::Dict::Lookup<LogGroup>(logevent, "log_group");
    auto category = glib2::Value::Dict::Lookup<LogCategory>(logevent, "log_category");
    auto message = glib2::Value::Dict::Lookup<std::string>(logevent,
                                                           "log_message");
    try
    {
        auto session_token = glib2::Value::Dict::Lookup<std::string>(logevent,
                                                                     "log_session_token");
        return Log(group, category, session_token, message);
    }
    catch (const glib2::Utils::Exception &)
    {
        // This is fine; the log_session_token may not be available
        // and then we treat this event as a "normal" LogEvent without
        // the sessoin token value
    }
    return Log(group, category, message, false);
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
 *
 * @return Events::Log object of the parsed GVariant object
 */
Log parse_tuple(GVariant *logevent, bool with_session_token)
{
    if (!with_session_token)
    {
        glib2::Utils::checkParams(__func__, logevent, "(uus)", 3);
        auto group = glib2::Value::Extract<LogGroup>(logevent, 0);
        auto category = glib2::Value::Extract<LogCategory>(logevent, 1);
        auto message = glib2::Value::Extract<std::string>(logevent, 2);
        return Log(group, category, message, false);
    }
    else
    {
        glib2::Utils::checkParams(__func__, logevent, "(uuss)", 4);
        auto group = glib2::Value::Extract<LogGroup>(logevent, 0);
        auto category = glib2::Value::Extract<LogCategory>(logevent, 1);
        auto session_token = glib2::Value::Extract<std::string>(logevent, 2);
        auto message = glib2::Value::Extract<std::string>(logevent, 3);
        return Log(group, category, session_token, message, false);
    }
}



} // Anonymous namespace


Log ParseLog(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &sess_token,
             const std::string &msg,
             bool filter_nl)
{
    auto [grp, ctg] = parse_group_category(grp_s, ctg_s);
    return sess_token.empty()
               ? Log(grp, ctg, msg, filter_nl)
               : Log(grp, ctg, sess_token, msg, filter_nl);
}


Log ParseLog(const std::string &grp_s,
             const std::string &ctg_s,
             const std::string &msg,
             bool filter_nl)
{
    return ParseLog(grp_s, ctg_s, {}, msg, filter_nl);
}


Log ParseLog(GVariant *logev, DBus::Signals::Target::Ptr sndr)
{
    if (nullptr != logev)
    {
        std::string g_type(g_variant_get_type_string(logev));
        Log event;
        if ("a{sv}" == g_type)
        {
            event = parse_dict(logev);
        }
        else if ("(uus)" == g_type)
        {
            event = parse_tuple(logev, false);
        }
        else if ("(uuss)" == g_type)
        {
            event = parse_tuple(logev, true);
        }
        else
        {
            throw LogException("LogEvent: Invalid LogEvent data type");
        }

        if (sndr)
        {
            event.SetDBusSender(sndr);
        }
        return event;
    }
    throw LogException("LogEvent: Invalid LogEvent GVariant data");
}


} // namespace Events
