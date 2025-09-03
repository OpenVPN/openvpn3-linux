//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logevent-selftest.cpp
 *
 * @brief  Unit test for struct LogEvent
 */

#include <iostream>
#include <string>
#include <sstream>
#include <gdbuspp/glib2/utils.hpp>

#include "events/status.hpp"
#include "events/log.hpp"


bool test_empty(const Events::Log &ev, const bool expect)
{
    bool ret = false;

    bool r = ev.empty();
    std::cout << "      test_empty():  ev.empty() = " << r << " ... ";
    if (expect != r)
    {
        std::cerr << "** ERROR **  Object not empty [empty()]: " << std::endl;
        ret = true;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }


    r = (LogGroup::UNDEFINED == ev.group
         && LogCategory::UNDEFINED == ev.category
         && ev.message.empty());
    std::cout << "      test_empty():  Element check:"
              << " (" << std::to_string(static_cast<uint8_t>(ev.group))
              << ", " << std::to_string(static_cast<uint8_t>(ev.category))
              << ", '" << ev.message << "') = " << r << " ... ";
    if (expect != r)
    {
        std::cerr << "** ERROR **  Object not empty [direct member check]"
                  << std::endl;
        ret = false;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    return ret;
};


int test1()
{
    int ret = 0;

    std::cout << "-- Testing just initialized object - empty" << std::endl;
    Events::Log empty;

    if (test_empty(empty, true))
    {
        ++ret;
    }

    std::cout << "-- Testing just initialized object - init with values (1)" << std::endl;
    Events::Log populated1(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    if (test_empty(populated1, false)) // This should fail
    {
        ++ret;
    }

    std::cout << "-- Testing just object with values being cleared" << std::endl;
    populated1.reset();
    if (test_empty(populated1, true))
    {
        ++ret;
    }

    std::cout << "-- Testing initialization with session token" << std::endl;
    Events::Log logev(LogGroup::LOGGER, LogCategory::INFO, "session_token_value", "Log message");
    if (test_empty(logev, false))
    {
        ++ret;
    }

    std::cout << "-- Testing reset() on object with session token" << std::endl;
    logev.reset();
    if (test_empty(logev, true))
    {
        ++ret;
    }

    return ret;
}


int test2_without_session_token()
{
    int ret = 0;
    GVariant *data = nullptr;
    try
    {
        std::cout << "-- Testing parsing GVariantDict - incorrect data ... ";
        data = g_variant_new("(uuis)",
                             static_cast<uint32_t>(StatusMajor::CONFIG),
                             static_cast<uint32_t>(StatusMinor::CFG_OK),
                             1234,
                             "Invalid data");
        auto parsed = Events::ParseLog(data);
        std::cout << "FAILED - should not be parsed successfully." << std::endl;
        ++ret;
    }
    catch (LogException &excp)
    {
        std::string err(excp.what());
        if (err.find("LogEvent: Invalid LogEvent data type") == std::string::npos)
        {
            std::cout << "FAILED - Incorrect exception: '" << err << "'"
                      << std::endl;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
    }
    if (nullptr != data)
    {
        g_variant_unref(data);
    }

    try
    {
        std::cout << "-- Testing parsing GVariantDict - correct dict ... ";
        GVariantBuilder *b = glib2::Builder::Create("a{sv}");
        g_variant_builder_add(b, "{sv}", "log_group", glib2::Value::Create(LogGroup::LOGGER));
        g_variant_builder_add(b, "{sv}", "log_category", glib2::Value::Create(LogCategory::DEBUG));
        g_variant_builder_add(b, "{sv}", "log_message", glib2::Value::Create<std::string>("Test log message"));
        GVariant *data = glib2::Builder::Finish(b);
        auto parsed = Events::ParseLog(data);

        if (LogGroup::LOGGER != parsed.group
            || LogCategory::DEBUG != parsed.category
            || 0 != parsed.message.compare("Test log message"))
        {
            std::cout << "FAILED: GVariant parsing failed:" << parsed
                      << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
        g_variant_unref(data);
    }
    catch (LogException &excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    try
    {
        std::cout << "-- Testing parsing GVariant Tuple (uus) ... ";
        GVariant *data = g_variant_new("(uus)",
                                       static_cast<uint32_t>(LogGroup::BACKENDPROC),
                                       static_cast<uint32_t>(LogCategory::INFO),
                                       "Parse testing again");
        auto parsed = Events::ParseLog(data);
        g_variant_unref(data);

        if (LogGroup::BACKENDPROC != parsed.group
            || LogCategory::INFO != parsed.category
            || 0 != parsed.message.compare("Parse testing again"))
        {
            std::cout << "FAILED" << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
    }
    catch (LogException &excp)
    {
        std::string err(excp.what());
        if (err.find("LogEvent: Invalid LogEvent data type") == std::string::npos)
        {
            std::cout << "FAILED - Incorrect exception: '" << err << "'"
                      << std::endl;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
    }


    std::cout << "-- Testing .GetGVariantTuple() ... ";
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    LogGroup grp = glib2::Value::Extract<LogGroup>(revparse, 0);
    LogCategory ctg = glib2::Value::Extract<LogCategory>(revparse, 1);
    std::string msg = glib2::Value::Extract<std::string>(revparse, 2);

    if (reverse.group != grp
        || reverse.category != ctg
        || reverse.message != msg)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << reverse << std::endl;
        std::cout << "    Output: "
                  << "group=" << std::to_string(static_cast<uint32_t>(grp)) << ", "
                  << "category=" << std::to_string(static_cast<uint32_t>(ctg)) << ", "
                  << "message='" << msg << "'" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_variant_unref(revparse);

    try
    {
        std::cout << "-- Testing .GetGVariantDict() ... ";
        Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "Moar testing is needed");
        GVariant *revparse = dicttest.GetGVariantDict();

        // Reuse the parser in LogEvent.  As that has already passed the
        // test, expect this to work too.
        auto cmp = Events::ParseLog(revparse);

        if (dicttest.group != cmp.group
            || dicttest.category != cmp.category
            || 0 != dicttest.message.compare(cmp.message))
        {
            std::cout << "FAILED" << std::endl;
            std::cout << "     Input: " << dicttest << std::endl;
            std::cout << "    Output: " << cmp << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
        g_variant_unref(revparse);
    }
    catch (LogException &excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    return ret;
}


int test2_with_session_token()
{
    int ret = 0;
    try
    {
        std::cout << "-- Testing parsing GVariantDict - with session token ... ";
        GVariantBuilder *b = glib2::Builder::Create("a{sv}");
        g_variant_builder_add(b, "{sv}", "log_group", glib2::Value::Create(LogGroup::LOGGER));
        g_variant_builder_add(b, "{sv}", "log_category", glib2::Value::Create(LogCategory::DEBUG));
        g_variant_builder_add(b, "{sv}", "log_session_token", glib2::Value::Create<std::string>("session_token_value"));
        g_variant_builder_add(b, "{sv}", "log_message", glib2::Value::Create<std::string>("Test log message"));
        GVariant *data = glib2::Builder::Finish(b);
        auto parsed = Events::ParseLog(data);

        if (LogGroup::LOGGER != parsed.group
            || LogCategory::DEBUG != parsed.category
            || 0 != parsed.session_token.compare("session_token_value")
            || 0 != parsed.message.compare("Test log message"))
        {
            std::cout << "FAILED: GVariant parsing failed:" << parsed
                      << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
        g_variant_unref(data);
    }
    catch (LogException &excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    try
    {
        std::cout << "-- Testing parsing GVariant Tuple with session token (uuss) ... ";
        GVariant *data = g_variant_new("(uuss)",
                                       static_cast<LogGroup>(LogGroup::BACKENDPROC),
                                       static_cast<LogCategory>(LogCategory::INFO),
                                       "session_token_val",
                                       "Parse testing again");
        auto parsed = Events::ParseLog(data);
        g_variant_unref(data);

        if (LogGroup::BACKENDPROC != parsed.group
            || LogCategory::INFO != parsed.category
            || 0 != parsed.session_token.compare("session_token_val")
            || 0 != parsed.message.compare("Parse testing again"))
        {
            std::cout << "FAILED" << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
    }
    catch (LogException &excp)
    {
        std::string err(excp.what());
        if (err.find("LogEvent: Invalid LogEvent data type") == std::string::npos)
        {
            std::cout << "FAILED - Incorrect exception: '" << err << "'"
                      << std::endl;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
    }


    std::cout << "-- Testing .GetGVariantTuple() with session token ... ";
    Events::Log reverse(LogGroup::BACKENDSTART, LogCategory::WARN, "YetAnotherSessionToken", "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();

    LogGroup grp = glib2::Value::Extract<LogGroup>(revparse, 0);
    LogCategory ctg = glib2::Value::Extract<LogCategory>(revparse, 1);
    std::string sesstok = glib2::Value::Extract<std::string>(revparse, 2);
    std::string msg = glib2::Value::Extract<std::string>(revparse, 3);

    if (reverse.group != grp
        || reverse.category != ctg
        || reverse.session_token != sesstok
        || reverse.message != msg)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << reverse << std::endl;
        std::cout << "    Output: "
                  << "group=" << std::to_string(static_cast<uint32_t>(grp)) << ", "
                  << "category=" << std::to_string(static_cast<uint32_t>(ctg)) << ", "
                  << "message='" << msg << "'" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_variant_unref(revparse);

    try
    {
        std::cout << "-- Testing .GetGVariantDict() with session_token ... ";
        Events::Log dicttest(LogGroup::CLIENT, LogCategory::ERROR, "MoarSessionTokens", "Moar testing is needed");
        GVariant *revparse = dicttest.GetGVariantDict();

        // Reuse the parser in LogEvent.  As that has already passed the
        // test, expect this to work too.
        auto cmp = Events::ParseLog(revparse);

        if (dicttest.group != cmp.group
            || dicttest.category != cmp.category
            || 0 != dicttest.session_token.compare(cmp.session_token)
            || 0 != dicttest.message.compare(cmp.message))
        {
            std::cout << "FAILED" << std::endl;
            std::cout << "     Input: " << dicttest << std::endl;
            std::cout << "    Output: " << cmp << std::endl;
            ++ret;
        }
        else
        {
            std::cout << "PASSED" << std::endl;
        }
        g_variant_unref(revparse);
    }
    catch (LogException &excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    return ret;
}


bool test_compare(const Events::Log &lhs, const Events::Log &rhs, const bool expect)
{
    bool ret = false;
    std::cout << "-- Compare check: {" << lhs << "} == {" << rhs << "} returns "
              << (expect ? "true" : "false") << " ... ";
    if ((lhs == rhs) == expect)
    {
        std::cout << "PASSED" << std::endl;
        ret = true;
    }
    else
    {
        std::cout << "FAILED" << std::endl;
        ret = false;
    }

    std::cout << "-- Compare check: {" << lhs << "} != {" << rhs << "} returns "
              << (expect ? "false" : "true") << " ... ";
    if ((lhs != rhs) != expect)
    {
        std::cout << "PASSED" << std::endl;
        ret &= true;
    }
    else
    {
        std::cout << "FAILED" << std::endl;
        ret &= false;
    }
    return ret;
}


int test3()
{
    int ret = 0;

    Events::Log ev1(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    Events::Log cmp1(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    if (!test_compare(ev1, cmp1, true))
    {
        ++ret;
    }

    Events::Log ev2(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    Events::Log cmp2(LogGroup::BACKENDPROC, LogCategory::DEBUG, "var1");
    if (!test_compare(ev2, cmp2, false))
    {
        ++ret;
    }

    Events::Log ev3(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp3(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    if (!test_compare(ev3, cmp3, false))
    {
        ++ret;
    }

    Events::Log ev4(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    Events::Log cmp4(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
    if (!test_compare(ev4, cmp4, false))
    {
        ++ret;
    }

    return ret;
}


int test4()
{
    int ret = 0;

    std::cout << "-- Testing string stream: LogEvent(LogGroup::LOGGER, "
              << "LogCategory::DEBUG, \"Debug message\") ... ";
    Events::Log logev(LogGroup::LOGGER, LogCategory::DEBUG, "Debug message");
    std::stringstream chk;
    chk << logev;
    std::string expect("Logger DEBUG: Debug message");
    if (chk.str() != expect)
    {
        std::cout << "FAILED: {" << logev << "}" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }

    return ret;
}



int main(int argc, char **argv)
{
    bool failed = false;
    int r = 0;

    if ((r = test1()) > 0)
    {
        std::cout << "** test1() failed, result: " << r << std::endl;
        failed = true;
    }

    if ((r = test2_without_session_token()) > 0)
    {
        std::cout << "** test2_without_session_token() failed" << std::endl;
        failed = true;
    }

    if ((r = test2_with_session_token()) > 0)
    {
        std::cout << "** test2_with_session_token() failed" << std::endl;
        failed = true;
    }

    if ((r = test3()) > 0)
    {
        std::cout << "** test3() failed" << std::endl;
        failed = true;
    }

    if ((r = test4()) > 0)
    {
        std::cout << "** test4() failed" << std::endl;
        failed = true;
    }


    std::cout << std::endl
              << std::endl
              << ">> OVERALL TEST RESULT: " << (failed ? "FAILED" : "PASSED")
              << std::endl
              << std::endl;
    return (failed ? 2 : 0);
}
