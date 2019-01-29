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
 * @file   logevent-selftest.cpp
 *
 * @brief  Unit test for struct LogEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include "dbus/core.hpp"
#include "log/logevent.hpp"


bool test_empty(const LogEvent& ev, const bool expect)
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
              << " (" << std::to_string((unsigned) ev.group)
              << ", " << std::to_string((unsigned) ev.category)
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
    LogEvent empty;

    if (test_empty(empty, true))
    {
        ++ret;
    }

    std::cout << "-- Testing just initialized object - init with values (1)" << std::endl;
    LogEvent populated1(LogGroup::LOGGER, LogCategory::DEBUG, "Test LogEvent");
    if (test_empty(populated1, false))  // This should fail
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
    LogEvent logev(LogGroup::LOGGER, LogCategory::INFO,
                   "session_token_value", "Log message");
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
                                       (guint) StatusMajor::CONFIG,
                                       (guint) StatusMinor::CFG_OK,
                                       1234, "Invalid data");
        LogEvent parsed(data);
        std::cout << "FAILED - should not be parsed successfully." << std::endl;
        ++ret;
    }
    catch (LogException& excp)
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
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (b, "{sv}", "log_group",
                               g_variant_new_uint32((guint) LogGroup::LOGGER));
        g_variant_builder_add (b, "{sv}", "log_category",
                               g_variant_new_uint32((guint) LogCategory::DEBUG));
        g_variant_builder_add (b, "{sv}", "log_message",
                               g_variant_new_string("Test log message"));
        GVariant *data= g_variant_builder_end(b);
        g_variant_builder_unref(b);
        LogEvent parsed(data);

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
    catch (LogException& excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    try
    {
        std::cout << "-- Testing parsing GVariant Tuple (uus) ... ";
        GVariant *data = g_variant_new("(uus)",
                                       (guint) LogGroup::BACKENDPROC,
                                       (guint) LogCategory::INFO ,
                                       "Parse testing again");
        LogEvent parsed(data);
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
    catch (LogException& excp)
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
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN,
                        "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();
    guint grp = 0;
    guint ctg = 0;
    gchar *msg = nullptr;
    g_variant_get(revparse, "(uus)", &grp, &ctg, &msg);

    if ((guint) reverse.group != grp
        || (guint) reverse.category != ctg
        || g_strcmp0(reverse.message.c_str(), msg) != 0)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << reverse << std::endl;
        std::cout << "    Output: "
                  << "group=" << std::to_string(grp) << ", "
                  << "category=" << std::to_string(ctg) << ", "
                  << "message='" << msg << "'" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_free(msg);
    g_variant_unref(revparse);

    try
    {
        std::cout << "-- Testing .GetGVariantDict() ... ";
        LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR,
                             "Moar testing is needed");
        GVariant *revparse = dicttest.GetGVariantDict();

        // Reuse the parser in LogEvent.  As that has already passed the
        // test, expect this to work too.
        LogEvent cmp(revparse);

        if (dicttest.group != cmp.group
            || dicttest.category != cmp.category
            || 0!= dicttest.message.compare(cmp.message))
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
    catch (LogException& excp)
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
    catch (LogException& excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    try
    {
        std::cout << "-- Testing parsing GVariant Tuple with session token (uuss) ... ";
        GVariant *data = g_variant_new("(uuss)",
                                       (guint) LogGroup::BACKENDPROC,
                                       (guint) LogCategory::INFO,
                                       "session_token_val",
                                       "Parse testing again");
        LogEvent parsed(data);
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
    catch (LogException& excp)
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
    LogEvent reverse(LogGroup::BACKENDSTART, LogCategory::WARN,
                     "YetAnotherSessionToken", "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();
    guint grp = 0;
    guint ctg = 0;
    gchar *sesstok = nullptr;
    gchar *msg = nullptr;
    g_variant_get(revparse, "(uuss)", &grp, &ctg, &sesstok, &msg);

    if ((guint) reverse.group != grp
        || (guint) reverse.category != ctg
        || g_strcmp0(reverse.session_token.c_str(), sesstok) != 0
        || g_strcmp0(reverse.message.c_str(), msg) != 0)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << reverse << std::endl;
        std::cout << "    Output: "
                  << "group=" << std::to_string(grp) << ", "
                  << "category=" << std::to_string(ctg) << ", "
                  << "message='" << msg << "'" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }
    g_free(sesstok);
    g_free(msg);
    g_variant_unref(revparse);

    try
    {
        std::cout << "-- Testing .GetGVariantDict() with session_token ... ";
        LogEvent dicttest(LogGroup::CLIENT, LogCategory::ERROR,
                          "MoarSessionTokens", "Moar testing is needed");
        GVariant *revparse = dicttest.GetGVariantDict();

        // Reuse the parser in LogEvent.  As that has already passed the
        // test, expect this to work too.
        LogEvent cmp(revparse);

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
    catch (LogException& excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    return ret;
}


bool test_compare(const LogEvent& lhs, const LogEvent& rhs, const bool expect)
{
    bool ret = false;
    std::cout << "-- Compare check: {" << lhs << "} == {" << rhs<< "} returns "
               << (expect ? "true": "false") << " ... ";
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
               << (expect ? "false": "true") << " ... ";
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

    LogEvent ev1(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    LogEvent cmp1(LogGroup::SESSIONMGR, LogCategory::FATAL, "var1");
    if (!test_compare(ev1, cmp1, true))
    {
        ++ret;
    }

    LogEvent ev2(LogGroup::BACKENDSTART, LogCategory::DEBUG, "var1");
    LogEvent cmp2(LogGroup::BACKENDPROC,  LogCategory::DEBUG, "var1");
    if (!test_compare(ev2, cmp2, false))
    {
        ++ret;
    }

    LogEvent ev3(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp3(LogGroup::CONFIGMGR, LogCategory::WARN, "var4");
    if (!test_compare(ev3, cmp3, false))
    {
        ++ret;
    }

    LogEvent ev4(LogGroup::CONFIGMGR, LogCategory::ERROR, "var4");
    LogEvent cmp4(LogGroup::CONFIGMGR, LogCategory::ERROR, "different");
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
    LogEvent logev(LogGroup::LOGGER, LogCategory::DEBUG,
                       "Debug message");
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


    std::cout << std::endl << std::endl
              << ">> OVERAL TEST RESULT: " << (failed ? "FAILED" : "PASSED")
              << std::endl << std::endl;
    return (failed ? 2: 0);
}
