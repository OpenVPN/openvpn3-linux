//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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
#include "log/log-helpers.hpp"

namespace unittest {

std::string test_empty(const LogEvent &ev, const bool expect)
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
               + "(" + std::to_string((unsigned)ev.group) + ", "
               + std::to_string((unsigned)ev.category) + "', "
               + "'" + ev.message + "', "
               + "message.size=" + std::to_string(ev.message.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " + (expect ? "EMPTY" : "NON-EMPTY") + "]";
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
    LogEvent ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::SESSION_TOKEN);
}


TEST(LogEvent, reset_with_session_token)
{
    LogEvent ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, LogEvent::Format::AUTO);
}


TEST(LogEvent, log_group_str)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        LogEvent ev((LogGroup)g, LogCategory::UNDEFINED, "irrelevant message");
        std::string expstr((g < LogGroupCount ? LogGroup_str[g] : "[group:10]"));
        EXPECT_STREQ(ev.GetLogGroupStr().c_str(), expstr.c_str());
    }
}


TEST(LogEvent, log_category_str)
{
    for (uint8_t c = 0; c < 10; c++)
    {
        LogEvent ev(LogGroup::UNDEFINED, (LogCategory)c, "irrelevant message");
        std::string expstr((c < 9 ? LogCategory_str[c] : "[category:9]"));
        EXPECT_STREQ(ev.GetLogCategoryStr().c_str(), expstr.c_str());
    }
}

TEST(LogEvent, parse_gvariant_tuple_invalid)
{
    GVariant *data = g_variant_new("(uuis)",
                                   (guint)StatusMajor::CONFIG,
                                   (guint)StatusMinor::CFG_OK,
                                   1234,
                                   "Invalid data");
    ASSERT_THROW(LogEvent parsed(data), LogException);
    if (nullptr != data)
    {
        g_variant_unref(data);
    }
}


TEST(LogEvent, parse_gvariant_dict)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(b, "{sv}", "log_group", g_variant_new_uint32((guint)LogGroup::LOGGER));
    g_variant_builder_add(b, "{sv}", "log_category", g_variant_new_uint32((guint)LogCategory::DEBUG));
    g_variant_builder_add(b, "{sv}", "log_message", g_variant_new_string("Test log message"));
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
                                   (guint)LogGroup::BACKENDPROC,
                                   (guint)LogCategory::INFO,
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
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    guint grp = 0;
    guint ctg = 0;
    gchar *msg_c = nullptr;
    g_variant_get(revparse, "(uus)", &grp, &ctg, &msg_c);
    std::string msg(msg_c);
    g_free(msg_c);

    ASSERT_EQ(reverse.group, (LogGroup)grp);
    ASSERT_EQ(reverse.category, (LogCategory)ctg);
    ASSERT_EQ(reverse.message, msg);
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict)
{
    LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR, "Moar testing is needed");
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
    g_variant_builder_add(b, "{sv}", "log_group", g_variant_new_uint32((guint)LogGroup::LOGGER));
    g_variant_builder_add(b, "{sv}", "log_category", g_variant_new_uint32((guint)LogCategory::DEBUG));
    g_variant_builder_add(b, "{sv}", "log_session_token", g_variant_new_string("session_token_value"));
    g_variant_builder_add(b, "{sv}", "log_message", g_variant_new_string("Test log message"));
    GVariant *data = g_variant_builder_end(b);
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
                                   (guint)LogGroup::BACKENDPROC,
                                   (guint)LogCategory::INFO,
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
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "YetAnotherSessionToken", "Yet another test");
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


    ASSERT_EQ((LogGroup)grp, reverse.group);
    ASSERT_EQ((LogCategory)ctg, reverse.category);
    ASSERT_EQ(sesstok, reverse.session_token);
    ASSERT_EQ(msg, reverse.message);
    g_variant_unref(revparse);
}


TEST(LogEvent, GetVariantDict_session_token)
{
    LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR, "MoarSessionTokens", "Moar testing is needed");
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


std::string test_compare(const LogEvent &lhs, const LogEvent &rhs, const bool expect)
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
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
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
            << (r ? "true" : "false")
            << " - expected: "
            << (expect ? "true" : "false");
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
    LogEvent cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_group)
{
    LogEvent ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    LogEvent cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
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


TEST(LogEvent, group_category_string_parser)
{
    for (uint8_t g = 0; g < LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 9; c++)
        {
            std::string msg1 = "Message without session token";
            LogEvent ev1(LogGroup_str[g], LogCategory_str[c], msg1);
            LogEvent chk_ev1((LogGroup)g, (LogCategory)c, msg1);
            std::string res1 = test_compare(ev1, chk_ev1, true);
            EXPECT_TRUE(res1.empty()) << res1;

            std::string msg2 = "Message with session token";
            std::string sesstok = "SESSION_TOKEN";
            LogEvent ev2(LogGroup_str[g], LogCategory_str[c], sesstok, msg2);
            LogEvent chk_ev2((LogGroup)g, (LogCategory)c, sesstok, msg2);
            std::string res2 = test_compare(ev2, chk_ev2, true);
            EXPECT_TRUE(res2.empty()) << res2;
        }
    }
}


TEST(LogEvent, stringstream)
{
    LogEvent logev(LogGroup::LOGGER, LogCategory::DEBUG, "Debug message");
    std::stringstream chk;
    chk << logev;
    std::string expect("Logger DEBUG: Debug message");
    ASSERT_EQ(chk.str(), expect);
}


TEST(LogEvent, stringstream_multiline)
{
    // Check formatting without LogPrefix and no indenting of NL
    std::stringstream msg1;
    msg1 << "Log line 1" << std::endl
         << "Log line 2" << std::endl
         << "Log Line 3";
    LogEvent ev1(LogGroup::LOGGER, LogCategory::DEBUG, msg1.str());
    EXPECT_EQ(ev1.str(0, false), msg1.str());

    // Check formatting with LogPrefix and no indenting of NL
    std::stringstream msg1prfx;
    msg1prfx << LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG) << msg1.str();
    EXPECT_EQ(ev1.str(), msg1prfx.str());

    // Check formatting via stream with LogPrefix and no indenting of NL
    std::stringstream ev1_chk0;
    ev1_chk0 << ev1;
    EXPECT_EQ(ev1_chk0.str(), msg1prfx.str());

    // Check formatting without LogPrefix and 5 space indenting of NL
    std::stringstream msg1ind5;
    msg1ind5 << "Log line 1" << std::endl
             << "     Log line 2" << std::endl
             << "     Log Line 3";
    EXPECT_EQ(ev1.str(5, false), msg1ind5.str());

    // Check formatting with LogPrefix and 5 space indenting of NL, default str()
    std::stringstream msg1indprfx;
    msg1indprfx << LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG) << msg1ind5.str();
    ev1.indent_nl = 5;
    EXPECT_EQ(ev1.str(), msg1indprfx.str());

    // Check formatting via stream with LogPrefix and 5 space indenting of NL (default formatting)
    std::stringstream ev1_chk5;
    ev1_chk5 << ev1;
    EXPECT_EQ(ev1_chk5.str(), msg1indprfx.str());
}


TEST(LogEvent, stringstream_grp_ctg_limits)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 10; c++)
        {
            LogEvent ev((LogGroup)g, (LogCategory)c, "some message string");
            std::stringstream msg;
            msg << ev;

            // Construct expected string manually
            std::stringstream chk;
            if (LogGroup::UNDEFINED != (LogGroup)g)
            {
                chk << (g < LogGroupCount ? LogGroup_str[g] : "[group:10]");
            }
            if (LogCategory::UNDEFINED != (LogCategory)c)
            {
                if (LogGroup::UNDEFINED != (LogGroup)g)
                {
                    chk << " ";
                }
                chk << (c < 9 ? LogCategory_str[c] : "[category:9]");
            }
            if (LogGroup::UNDEFINED != (LogGroup)g
                || LogCategory::UNDEFINED != (LogCategory)c)
            {
                chk << ": ";
            }
            chk << "some message string";
            EXPECT_EQ(ev.str(), chk.str());
        }
    }
}

} // namespace unittest
