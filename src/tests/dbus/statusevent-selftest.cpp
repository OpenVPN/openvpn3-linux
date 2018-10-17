//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   statusevent-selftest.cpp
 *
 * @brief  Unit test for struct StatusEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include "dbus/core.hpp"
#include "client/statusevent.hpp"


bool test_empty(const StatusEvent& ev, const bool expect)
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



    r = (StatusMajor::UNSET == ev.major
         && StatusMinor::UNSET == ev.minor
         && ev.message.empty());
    std::cout << "      test_empty():  Element check:"
              << " (" << std::to_string((unsigned) ev.major)
              << ", " << std::to_string((unsigned) ev.minor)
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
    StatusEvent empty;

    if (test_empty(empty, true))
    {
        ++ret;
    }

    std::cout << "-- Testing just initialized object - init with values (1)" << std::endl;
    StatusEvent populated1(StatusMajor::PROCESS, StatusMinor::PROC_STARTED);
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

    std::cout << "-- Testing just initialized object - init with values (2)" << std::endl;
    StatusEvent populated2(StatusMajor::PROCESS, StatusMinor::PROC_STOPPED,
                           "Just testing");
    if (test_empty(populated2, false))  // This should fail
    {
        ++ret;
    }

    std::cout << "-- Testing just object with values being cleared" << std::endl;
    populated1.reset();
    if (test_empty(populated1, true))  // This should fail
    {
        ++ret;
    }

    return ret;
}


int test2()
{
    int ret = 0;
    GVariant *data = nullptr;
    try
    {
        std::cout << "-- Testing parsing GVariantDict - incorrect data ... ";
        data = g_variant_new("(uuss)",
                                       (guint) StatusMajor::CONFIG,
                                       (guint) StatusMinor::CFG_OK,
                                       "Test status", "Invalid data");
        StatusEvent parsed(data);
        std::cout << "FAILED - should not be parsed successfully." << std::endl;
        ++ret;
    }
    catch (DBusException& excp)
    {
        std::string err(excp.what());
        if (err.find(" Invalid status data") == std::string::npos)
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
        g_variant_builder_add (b, "{sv}", "major",
                               g_variant_new_uint32((guint) StatusMajor::CONFIG));
        g_variant_builder_add (b, "{sv}", "minor",
                               g_variant_new_uint32((guint) StatusMinor::CFG_OK));
        g_variant_builder_add (b, "{sv}", "status_message",
                               g_variant_new_string("Test status"));
        GVariant *data= g_variant_builder_end(b);
        g_variant_builder_unref(b);
        StatusEvent parsed(data);

        if (StatusMajor::CONFIG != parsed.major
            || StatusMinor::CFG_OK != parsed.minor
            || 0 != parsed.message.compare("Test status"))
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
    catch (DBusException& excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    try
    {
        std::cout << "-- Testing parsing GVariant Tuple ... ";
        GVariant *data = g_variant_new("(uus)",
                                       (guint) StatusMajor::CONFIG,
                                       (guint) StatusMinor::CFG_REQUIRE_USER,
                                       "Parse testing again");
        StatusEvent parsed(data);
        g_variant_unref(data);

        if (StatusMajor::CONFIG != parsed.major
            || StatusMinor::CFG_REQUIRE_USER != parsed.minor
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
    catch (DBusException& excp)
    {
        std::string err(excp.what());
        if (err.find(" Incorrect StatusEvent dict ") == std::string::npos)
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
    StatusEvent reverse(StatusMajor::CONNECTION, StatusMinor::CONN_INIT,
                        "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();
    guint maj = 0;
    guint min = 0;
    gchar *msg = nullptr;

    g_variant_get(revparse, "(uus)", &maj, &min, &msg);
    if ((guint) reverse.major != maj
        || (guint) reverse.minor != min
        || g_strcmp0(reverse.message.c_str(), msg) != 0)
    {
        std::cout << "FAILED" << std::endl;
        std::cout << "     Input: " << reverse << std::endl;
        std::cout << "    Output: "
                  << "maj=" << std::to_string(maj) << ", "
                  << "min=" << std::to_string(min) << ", "
                  << "msg='" << msg << "'" << std::endl;
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
        StatusEvent dicttest(StatusMajor::SESSION, StatusMinor::SESS_NEW,
                             "Moar testing is needed");
        GVariant *revparse = dicttest.GetGVariantDict();

        // Reuse the parser in StatusEvent.  As that has already passed the
        // test, expect this to work too.
        StatusEvent cmp(revparse);

        if (dicttest.major != cmp.major
            || dicttest.minor != cmp.minor
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
    catch (DBusException& excp)
    {
        std::cout << "FAILED: Exception thrown: " << excp.what() << std::endl;
        ++ret;
    }

    return ret;
}


bool test_compare(const StatusEvent& lhs, const StatusEvent& rhs, const bool expect)
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
    StatusEvent ev1(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");

    if (!test_compare(ev1, chk, true))
    {
        ++ret;
    }

    StatusEvent ev2(StatusMajor::SESSION, StatusMinor::PKCS11_DECRYPT, "var1");
    StatusEvent ev3(StatusMajor::SESSION, StatusMinor::SESS_BACKEND_COMPLETED,
                    "var1");
if (!test_compare(ev2, ev3, false))
    {
        ++ret;
    }

    StatusEvent ev4(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);
    StatusEvent cmp2(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);

    if (!test_compare(ev4, cmp2, true))
    {
        ++ret;
    }

    StatusEvent ev5(StatusMajor::SESSION, StatusMinor::CFG_REQUIRE_USER);
    StatusEvent ev6(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT);
    if (!test_compare(ev5, ev6, false))
    {
        ++ret;
    }

    if(!test_compare(ev1, ev6, false))
    {
        ++ret;
    }
    if (!test_compare(ev3, ev6, false))
    {
        ++ret;
    }
    if (!test_compare(ev2, ev5, false))
    {
        ++ret;
    }

    return ret;
}

int test4()
{
    int ret = 0;

    std::cout << "-- Testing string stream: StatusEvent(StatusMajor::CONFIG, "
              << "StatusMinor::CONN_CONNECTING, \"In progress\") ... ";
    StatusEvent status(StatusMajor::CONFIG, StatusMinor::CONN_CONNECTING,
                       "In progress");
    std::stringstream chk;
    chk << status;
    std::string expect("Configuration, Client connecting: In progress");
    if (chk.str() != expect)
    {
        std::cout << "FAILED: {" << status << "}" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }

    std::cout << "-- Testing string stream: StatusEvent(StatusMajor::CONFIG, "
              << "StatusMinor::CONN_CONNECTING, \"In progress\") "
              << "with numeric_status = true... ";
    std::stringstream chk1;
    status.show_numeric_status = true;
    chk1 << status;
    std::string expect1("[1,6] Configuration, Client connecting: In progress");
    if (chk1.str() != expect1)
    {
        std::cout << "FAILED: {" << status << "}" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }


    std::cout << "-- Testing string stream: StatusEvent(StatusMajor::SESSION, "
              << "StatusMinor::SESS_BACKEND_COMPLETED) ... ";
    StatusEvent status2(StatusMajor::SESSION,
                       StatusMinor::SESS_BACKEND_COMPLETED);
    std::stringstream chk2;
    status2.show_numeric_status = true;
    chk2 << status2;
    std::string expect2("[3,18] Session, Backend Session Object completed");
    if (chk2.str() != expect2)
    {
        std::cout << "FAILED: {" << status2 << "}" << std::endl;
        ++ret;
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }

    std::cout << "-- Testing string stream: StatusEvent(StatusMajor::SESSION, "
              << "StatusMinor::SESS_BACKEND_COMPLETED) with numeric_status "
              << "reset to false ... ";
    std::stringstream chk3;
    status2.show_numeric_status = false;
    chk3 << status2;
    std::string expect3("Session, Backend Session Object completed");
    if (chk3.str() != expect3)
    {
        std::cout << "FAILED: {" << status2 << "}" << std::endl;
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

    if ((r = test2()) > 0)
    {
        std::cout << "** test2() failed" << std::endl;
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
