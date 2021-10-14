//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   logevent.cpp
 *
 * @brief  Unit test for struct LogEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "dbus/core.hpp"
#include "log/logevent.hpp"

namespace unittest {

std::string test_empty(const LogEvent& ev, const bool expect)
{
    bool r = ev.empty();
    if (expect != r)
    {
        return std::string("test_empty():  ")
                + "ev.empty() = " + (r ? "true" : "false")
                + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (LogGroup::UNDEFINED == ev.group
         && LogCategory::UNDEFINED == ev.category
         && ev.message.empty());
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string((unsigned) ev.group) + ", "
               + std::to_string((unsigned) ev.category) + "', "
               + "'" + ev.message + "', "
               +  "message.size=" + std::to_string(ev.message.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " +  (expect ? "EMPTY" : "NON-EMPTY") + "]";
    }
    return "";
};


TEST(LogEvent, init_empty)
{
    LogEvent empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(empty.format, LogEvent::Format::AUTO);
}


TEST(LogEvent, init_with_value_1)
{
    LogEvent ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::NORMAL);
}


TEST(LogEvent, reset)
{
    LogEvent ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::AUTO);
}


TEST(LogEvent, init_with_session_token)
{
    LogEvent ev(LogGroup::LOGGER, LogCategory::INFO,
                "session_token_value", "Log message");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::SESSION_TOKEN);
}


TEST(LogEvent, reset_with_session_token)
{
    LogEvent ev(LogGroup::LOGGER, LogCategory::INFO,
                "session_token_value", "Log message");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::AUTO);
}


TEST(LogEvent, parse_gvariant_tuple_invalid)
{
    GVariant *data = g_variant_new("(uuis)",
                                   (guint) StatusMajor::CONFIG,
                                   (guint) StatusMinor::CFG_OK,
                                   1234, "Invalid data");
    ASSERT_THROW(LogEvent parsed(data), LogException);
    if (nullptr != data)
    {
        g_variant_unref(data);
    }
}


TEST(LogEvent, parse_gvariant_dict)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add (b, "{sv}", "log_group",
                           g_variant_new_uint32((guint) LogGroup::LOGGER));
    g_variant_builder_add(b, "{sv}", "log_category",
                          g_variant_new_uint32((guint) LogCategory::DEBUG));
    g_variant_builder_add(b, "{sv}", "log_message",
                          g_variant_new_string("Test log message"));
    GVariant *data = g_variant_builder_end(b);
    g_variant_builder_unref(b);

    LogEvent parsed(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.message, "Test log message");
    ASSERT_EQ(parsed.format, LogEvent::Format::NORMAL);
}


TEST(LogEvent, parse_gvariant_tuple)
{
    GVariant *data = g_variant_new("(uus)",
                                   (guint) LogGroup::BACKENDPROC,
                                   (guint) LogCategory::INFO ,
                                   "Parse testing again");
    LogEvent parsed(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
    ASSERT_EQ(parsed.category, LogCategory::INFO);
    ASSERT_EQ(parsed.message, "Parse testing again");
    ASSERT_EQ(parsed.format, LogEvent::Format::NORMAL);
}


TEST(LogEvent, GetVariantTuple)
{
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN,
                     "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    guint grp = 0;
    guint ctg = 0;
    gchar *msg_c = nullptr;
    g_variant_get(revparse, "(uus)", &grp, &ctg, &msg_c);
    std::string msg(msg_c);
    g_free(msg_c);

    ASSERT_EQ(reverse.group, (LogGroup) grp);
    ASSERT_EQ(reverse.category, (LogCategory) ctg);
    ASSERT_EQ(reverse.message, msg);
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict)
{
    LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR,
                      "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    LogEvent cmp(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.group, dicttest.group);
    ASSERT_EQ(cmp.category, dicttest.category);
    ASSERT_EQ(cmp.message, dicttest.message);
}


TEST(LogEvent, parse_gvariant_dict_session_token)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add (b, "{sv}", "log_group",
                           g_variant_new_uint32((guint) LogGroup::LOGGER));
    g_variant_builder_add (b, "{sv}", "log_category",
                           g_variant_new_uint32((guint) LogCategory::DEBUG));
    g_variant_builder_add (b, "{sv}", "log_session_token",
                           g_variant_new_string("session_token_value"));
    g_variant_builder_add (b, "{sv}", "log_message",
                           g_variant_new_string("Test log message"));
    GVariant *data= g_variant_builder_end(b);
    g_variant_builder_unref(b);
    LogEvent parsed(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.session_token, "session_token_value");
    ASSERT_EQ(parsed.message, "Test log message");
    ASSERT_EQ(parsed.format, LogEvent::Format::SESSION_TOKEN);
}


TEST(LogEvent, parse_gvariant_tuple_session_token)
{
        GVariant *data = g_variant_new("(uuss)",
                                       (guint) LogGroup::BACKENDPROC,
                                       (guint) LogCategory::INFO,
                                       "session_token_val",
                                       "Parse testing again");
        LogEvent parsed(data);
        g_variant_unref(data);

        ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
        ASSERT_EQ(parsed.category, LogCategory::INFO);
        ASSERT_EQ(parsed.session_token, "session_token_val");
        ASSERT_EQ(parsed.message, "Parse testing again");
        ASSERT_EQ(parsed.format, LogEvent::Format::SESSION_TOKEN);
}


TEST(LogEvent, GetVariantTuple_session_token)
{
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN,
                     "YetAnotherSessionToken", "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    guint grp = 0;
    guint ctg = 0;
    gchar *sesstok_c = nullptr;
    gchar *msg_c = nullptr;
    g_variant_get(revparse, "(uuss)", &grp, &ctg, &sesstok_c, &msg_c);

    std::string sesstok(sesstok_c);
    g_free(sesstok_c);
    std::string msg(msg_c);
    g_free(msg_c);


    ASSERT_EQ((LogGroup) grp, reverse.group);
    ASSERT_EQ((LogCategory) ctg, reverse.category);
    ASSERT_EQ(sesstok, reverse.session_token);
    ASSERT_EQ(msg, reverse.message);
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict_session_token)
{
    LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR,
                      "MoarSessionTokens", "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    LogEvent cmp(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.group, dicttest.group);
    ASSERT_EQ(cmp.category, dicttest.category);
    ASSERT_EQ(cmp.session_token, dicttest.session_token);
    ASSERT_EQ(cmp.message, dicttest.message);
    ASSERT_EQ(cmp.format, dicttest.format);
}


std::string test_compare(const LogEvent& lhs, const LogEvent& rhs, const bool expect)
{
    bool r = (lhs.group == rhs.group
              && lhs.category == rhs.category
              && lhs.session_token == rhs.session_token
              && lhs.message == rhs.message);
    if (r != expect)
    {
        std::stringstream err;
        err << "LogEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }

    r = (lhs.group != rhs.group
         || lhs.category != rhs.category
         || lhs.session_token != rhs.session_token
         || lhs.message != rhs.message);
    if (r == expect)
    {
        std::stringstream err;
        err << "Negative LogEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }
    return "";
}


TEST(LogEvent, compare_eq)
{
    LogEvent ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    LogEvent cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    std::string res = test_compare(ev, cmp, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_eq)
{
    LogEvent ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    LogEvent cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    ASSERT_TRUE(ev == cmp);
}


TEST(LogEvent, compare_neq_group)
{
    LogEvent ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    LogEvent cmp(LogGroup::BACKENDPROC,  LogCategory::DEBUG, "var1");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_group)
{
    LogEvent ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    LogEvent cmp(LogGroup::BACKENDPROC,  LogCategory::DEBUG, "var1");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_category)
{
    LogEvent ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_category)
{
    LogEvent ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_message)
{
    LogEvent ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_message)
{
    LogEvent ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, stringstream)
{
    LogEvent logev(LogGroup::LOGGER, LogCategory::DEBUG,
                       "Debug message");
    std::stringstream chk;
    chk << logev;
    std::string expect("Logger DEBUG: Debug message");
    ASSERT_EQ(chk.str(), expect);
}


} // namespace unittest
