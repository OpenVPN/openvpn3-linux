//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
    enum class Format : uint8_t
    {
        AUTO,
        NORMAL,
        SESSION_TOKEN
    };

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
             const std::string& msg)
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
    LogEvent(const LogGroup grp, const LogCategory ctg,
             const std::string& session_token, const std::string& msg)
        : group(grp), category(ctg),
          session_token(session_token), message(msg)
    {
        remove_trailing_nl();
        format = Format::SESSION_TOKEN;
    }


    LogEvent(const LogEvent& logev, const std::string& session_token)
        : group(logev.group), category(logev.category),
          session_token(session_token), message(logev.message)
    {
        remove_trailing_nl();
        format = Format::SESSION_TOKEN;
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
                THROW_LOGEXCEPTION("LogEvent: Invalid LogEvent data type");
            }
            remove_trailing_nl();
        }
    }


    void RemoveToken()
    {
        format = Format::NORMAL;
        session_token.clear();
    }


    static const std::string GetIntrospection(const std::string& name,
                                              bool with_session_token = false)
    {
        std::stringstream ret;
        ret << "        <signal name='" << name <<"'>"
            << "            <arg type='u' name='group' direction='out'/>"
            << "            <arg type='u' name='level' direction='out'/>";

        if (with_session_token)
        {
            ret << "            <arg type='s' name='session_token' direction='out'/>";
        }

        ret << "            <arg type='s' name='message' direction='out'/>"
            << "        </signal>";
        return std::string(ret.str());
    }


    /**
     *  Create a GVariant object containing a tuple formatted object for
     *  a Log signal of the current LogEvent.
     *
     * @return  Returns a pointer to a GVariant object with the formatted
     *          data
     */
    GVariant* GetGVariantTuple() const
    {
        if (Format::SESSION_TOKEN == format
            || (Format::AUTO == format && !session_token.empty()))
        {
            return g_variant_new("(uuss)", (guint32) group, (guint32) category,
                                 session_token.c_str(), message.c_str());
        }
        else
        {
            return g_variant_new("(uus)", (guint32) group, (guint32) category,
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
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(b, "{sv}", "log_group",
                              g_variant_new_uint32((guint) group));
        g_variant_builder_add(b, "{sv}", "log_category",
                              g_variant_new_uint32((guint) category));
        if (!session_token.empty() || Format::SESSION_TOKEN == format)
        {
            g_variant_builder_add(b, "{sv}", "log_session_token",
                                  g_variant_new_string(session_token.c_str()));
        }
        g_variant_builder_add(b, "{sv}", "log_message",
                              g_variant_new_string(message.c_str()));
        GVariant *ret = g_variant_builder_end(b);
        g_variant_builder_unref(b);
        return ret;
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


    bool operator==(const LogEvent& compare) const
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


    bool operator!=(const LogEvent& compare) const
    {
        return !(this->operator==(compare));
    }

    LogGroup group;
    LogCategory category;
    std::string session_token;
    std::string message;
    Format format = Format::AUTO;


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
        GVariant *d = nullptr;
        unsigned int v = 0;

        d = g_variant_lookup_value(logevent, "log_group", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        if (v > 0 && v < LogGroup_str.size())
        {
            group = (LogGroup) v;
        }
        g_variant_unref(d);
        d = nullptr;

        d = g_variant_lookup_value(logevent, "log_category", G_VARIANT_TYPE_UINT32);
        v = g_variant_get_uint32(d);
        if (v > 0 && v < LogCategory_str.size())
        {
            category = (LogCategory) v;
        }
        g_variant_unref(d);
        d = nullptr;

        gsize len;
        d = g_variant_lookup_value(logevent, "log_session_token",
                                   G_VARIANT_TYPE_STRING);
        if (d)
        {
            session_token = std::string(g_variant_get_string(d, &len));
            format = Format::SESSION_TOKEN;
            g_variant_unref(d);
        }
        else
        {
            format = Format::NORMAL;
        }
        d = nullptr;

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
        guint grp;
        guint ctg;
        gchar *sesstok = nullptr;
        gchar *msg = nullptr;

        if (!with_session_token)
        {
            g_variant_get(logevent, "(uus)", &grp, &ctg, &msg);
        }
        else
        {
            g_variant_get(logevent, "(uuss)", &grp, &ctg, &sesstok, &msg);
        }

        group = (LogGroup) grp;
        category = (LogCategory) ctg;
        if (sesstok)
        {
            session_token = std::string(sesstok);
            g_free(sesstok);
        }
        if (msg)
        {
            message = std::string(msg);
            g_free(msg);
        }
    }
};
