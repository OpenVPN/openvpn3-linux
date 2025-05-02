//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
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

#include "build-config.h"
#include "events/status.hpp"
#include "events/log.hpp"


namespace unittest {

std::string test_empty(const Events::Log &ev, const bool expect)
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
    Events::Log empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(empty.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, init_with_value_1)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, reset)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, init_with_session_token)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    std::string res = test_empty(ev, false);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, reset_with_session_token)
{
    Events::Log ev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    ev.reset();
    std::string res = test_empty(ev, true);
    ASSERT_TRUE(res.empty()) << res;
    ASSERT_EQ(ev.format, Events::Log::Format::AUTO);
}


TEST(LogEvent, log_group_str)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        Events::Log ev((LogGroup)g, LogCategory::UNDEFINED, "irrelevant message");
        std::string expstr((g < LogGroupCount ? LogGroup_str[g] : "[group:10]"));
        EXPECT_STREQ(ev.GetLogGroupStr().c_str(), expstr.c_str());
    }
}


TEST(LogEvent, log_category_str)
{
    for (uint8_t c = 0; c < 10; c++)
    {
        Events::Log ev(LogGroup::UNDEFINED, (LogCategory)c, "irrelevant message");
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
    ASSERT_THROW(auto parsed = Events::ParseLog(data), LogException);
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

    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.message, "Test log message");
    ASSERT_EQ(parsed.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, parse_gvariant_tuple)
{
    GVariant *data = g_variant_new("(uus)",
                                   (guint)LogGroup::BACKENDPROC,
                                   (guint)LogCategory::INFO,
                                   "Parse testing again");
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
    ASSERT_EQ(parsed.category, LogCategory::INFO);
    ASSERT_EQ(parsed.message, "Parse testing again");
    ASSERT_EQ(parsed.format, Events::Log::Format::NORMAL);
}


TEST(LogEvent, GetVariantTuple)
{
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "Yet another test");
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
    Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    auto cmp = Events::ParseLog(revparse);
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
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::LOGGER);
    ASSERT_EQ(parsed.category, LogCategory::DEBUG);
    ASSERT_EQ(parsed.session_token, "session_token_value");
    ASSERT_EQ(parsed.message, "Test log message");
    ASSERT_EQ(parsed.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, parse_gvariant_tuple_session_token)
{
    GVariant *data = g_variant_new("(uuss)",
                                   (guint)LogGroup::BACKENDPROC,
                                   (guint)LogCategory::INFO,
                                   "session_token_val",
                                   "Parse testing again");
    auto parsed = Events::ParseLog(data);
    g_variant_unref(data);

    ASSERT_EQ(parsed.group, LogGroup::BACKENDPROC);
    ASSERT_EQ(parsed.category, LogCategory::INFO);
    ASSERT_EQ(parsed.session_token, "session_token_val");
    ASSERT_EQ(parsed.message, "Parse testing again");
    ASSERT_EQ(parsed.format, Events::Log::Format::SESSION_TOKEN);
}


TEST(LogEvent, GetVariantTuple_session_token)
{
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "YetAnotherSessionToken", "Yet another test");
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
    Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "MoarSessionTokens", "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in LogEvent.  As that has already passed the
    // test, expect this to work too.
    auto cmp = Events::ParseLog(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.group, dicttest.group);
    ASSERT_EQ(cmp.category, dicttest.category);
    ASSERT_EQ(cmp.session_token, dicttest.session_token);
    ASSERT_EQ(cmp.message, dicttest.message);
    ASSERT_EQ(cmp.format, dicttest.format);
}


std::string test_compare(const Events::Log &lhs, const Events::Log &rhs, const bool expect)
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
    Events::Log ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    Events::Log cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    std::string res = test_compare(ev, cmp, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_eq)
{
    Events::Log ev(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    Events::Log cmp(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    ASSERT_TRUE(ev == cmp);
}


TEST(LogEvent, compare_neq_group)
{
    Events::Log ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    Events::Log cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_group)
{
    Events::Log ev(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    Events::Log cmp(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_category)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_category)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, compare_neq_message)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    std::string res = test_compare(ev, cmp, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(LogEvent, operator_neq_message)
{
    Events::Log ev(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    ASSERT_TRUE(ev != cmp);
}


TEST(LogEvent, group_category_string_parser)
{
    for (uint8_t g = 0; g < LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 9; c++)
        {
            std::string msg1 = "Message without session token";
            auto ev1 = Events::ParseLog(LogGroup_str[g], LogCategory_str[c], msg1);
            Events::Log chk_ev1((LogGroup)g, (LogCategory)c, msg1);
            std::string res1 = test_compare(ev1, chk_ev1, true);
            EXPECT_TRUE(res1.empty()) << res1;

            std::string msg2 = "Message with session token";
            std::string sesstok = "SESSION_TOKEN";
            auto ev2 = Events::ParseLog(LogGroup_str[g], LogCategory_str[c], sesstok, msg2);
            Events::Log chk_ev2((LogGroup)g, (LogCategory)c, sesstok, msg2);
            std::string res2 = test_compare(ev2, chk_ev2, true);
            EXPECT_TRUE(res2.empty()) << res2;
        }
    }
}


TEST(LogEvent, stringstream)
{
    Events::Log logev(LogGroup::LOGGER, LogCategory::DEBUG, "Debug message");
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
    Events::Log ev1(LogGroup::LOGGER, LogCategory::DEBUG, msg1.str(), false);
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

    // Check formatting filtering out newlines
    std::stringstream msg_nonl;
    msg_nonl << LogPrefix(LogGroup::LOGGER, LogCategory::DEBUG) << "Log line 1"
             << "Log line 2"
             << "Log Line 3";
    Events::Log ev_nonl(LogGroup::LOGGER, LogCategory::DEBUG, msg1.str());
    EXPECT_EQ(ev_nonl.str(), msg_nonl.str());
}


TEST(LogEvent, stringstream_grp_ctg_limits)
{
    for (uint8_t g = 0; g <= LogGroupCount; g++)
    {
        for (uint8_t c = 0; c < 10; c++)
        {
            Events::Log ev((LogGroup)g, (LogCategory)c, "some message string");
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


TEST(LogEvent, message_filter)
{
    // Checks that messages containing characters < 0x20 are filtered out
    std::string below_0x20{"This is a \1 test \x20with \b various \x0A\n\rcontrol "
                           "\x19\x0E\x1b[31m characters"};
    std::string expected_below_0x20_wo_nl{"This is a  test  with  various control [31m characters"};
    Events::Log ev_b0x20(LogGroup::LOGGER, LogCategory::DEBUG, below_0x20);
    EXPECT_EQ(ev_b0x20.str(0, false), expected_below_0x20_wo_nl);

    std::string expected_below_0x20_with_nl{"This is a  test  with  various \n\ncontrol [31m characters"};
    Events::Log ev_b0x20_w_nl(LogGroup::LOGGER, LogCategory::DEBUG, below_0x20, false);
    EXPECT_EQ(ev_b0x20_w_nl.str(0, false), expected_below_0x20_with_nl);

    std::string with_nl("This is line 1\nThis is line 2\nThis is line 3\n\n\n");
    // Trailing \n is always removed
    std::string expected_with_nl{"This is line 1\nThis is line 2\nThis is line 3"};
    Events::Log ev_with_nl(LogGroup::LOGGER, LogCategory::DEBUG, with_nl, false);
    EXPECT_EQ(ev_with_nl.str(0, false), expected_with_nl);

    std::string expected_wo_nl{"This is line 1This is line 2This is line 3"};
    Events::Log ev_wo_nl(LogGroup::LOGGER, LogCategory::DEBUG, with_nl, true);
    EXPECT_EQ(ev_wo_nl.str(0, false), expected_wo_nl);
}

} // namespace unittest
