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
 * @file   statusevent.cpp
 *
 * @brief  Unit test for struct StatusEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "dbus/core.hpp"
#include "client/statusevent.hpp"


namespace unittest {

std::string test_empty(const StatusEvent& ev, const bool expect)
{
    bool r = ev.empty();
    if (expect != r)
    {
        return std::string("test_empty():  ")
                + "ev.empty() = " + (r ? "true" : "false")
                + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (StatusMajor::UNSET == ev.major
         && StatusMinor::UNSET == ev.minor
         && ev.message.empty());
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string((unsigned) ev.major) + ", "
               + std::to_string((unsigned) ev.minor) + "', "
               +  "message.size=" + std::to_string(ev.message.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " +  (expect ? "EMPTY" : "NON-EMPTY") + "]";

    }
    return "";
};


TEST(StatusEvent, init_empty)
{
    StatusEvent empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, init_with_values)
{
    StatusEvent populated1(StatusMajor::PROCESS, StatusMinor::PROC_STARTED);
    std::string res = test_empty(populated1, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, reset)
{
    StatusEvent populated1(StatusMajor::PROCESS, StatusMinor::PROC_STARTED);
    populated1.reset();
    std::string res = test_empty(populated1, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, init_with_values_2)
{
    StatusEvent populated2(StatusMajor::PROCESS, StatusMinor::PROC_STOPPED,
                           "Just testing");
    std::string res = test_empty(populated2, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, reset_2)
{
    StatusEvent populated2(StatusMajor::PROCESS, StatusMinor::PROC_STOPPED,
                           "Just testing");
    populated2.reset();
    std::string res = test_empty(populated2, true);
    ASSERT_TRUE(res.empty()) << res;

}


TEST(StatusEvent, parse_gvariant_invalid_data)
{
    GVariant *data = nullptr;
    data = g_variant_new("(uuss)",
                         (guint) StatusMajor::CONFIG,
                         (guint) StatusMinor::CFG_OK,
                         "Test status", "Invalid data");

    ASSERT_THROW(StatusEvent parsed(data),
                 DBusException);
    if (nullptr != data)
    {
        g_variant_unref(data);
    }
}


TEST(StatusEvent, parse_gvariant_valid_dict)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add (b, "{sv}", "major",
                           g_variant_new_uint32((guint) StatusMajor::CONFIG));
    g_variant_builder_add (b, "{sv}", "minor",
                           g_variant_new_uint32((guint) StatusMinor::CFG_OK));
    g_variant_builder_add (b, "{sv}", "status_message",
                          g_variant_new_string("Test status"));
    GVariant *data = g_variant_builder_end(b);
    g_variant_builder_unref(b);

    StatusEvent parsed(data);
    ASSERT_EQ(parsed.major, StatusMajor::CONFIG);
    ASSERT_EQ(parsed.minor, StatusMinor::CFG_OK);
    ASSERT_EQ(parsed.message, "Test status");

    g_variant_unref(data);
}

TEST(StatusEvent, parse_gvariant_valid_tuple)
{
    GVariant *data = g_variant_new("(uus)",
                                   (guint) StatusMajor::CONFIG,
                                   (guint) StatusMinor::CFG_REQUIRE_USER,
                                   "Parse testing again");
    StatusEvent parsed(data);
    ASSERT_EQ(parsed.major, StatusMajor::CONFIG);
    ASSERT_EQ(parsed.minor, StatusMinor::CFG_REQUIRE_USER);
    ASSERT_EQ(parsed.message, "Parse testing again");

    g_variant_unref(data);
}


TEST(StatusEvent, GetGVariantTuple)
{
    StatusEvent reverse(StatusMajor::CONNECTION, StatusMinor::CONN_INIT,
                        "Yet another test");
    GVariant *revparse = reverse.GetGVariantTuple();
    guint maj = 0;
    guint min = 0;
    gchar *msg_c = nullptr;
    g_variant_get(revparse, "(uus)", &maj, &min, &msg_c);
    std::string msg(msg_c);
    g_free(msg_c);

    ASSERT_EQ((StatusMajor) maj, reverse.major);
    ASSERT_EQ((StatusMinor) min, reverse.minor);
    ASSERT_EQ(msg, reverse.message);

    g_variant_unref(revparse);
}


TEST(StatusEvent, GetVariantDict)
{
    StatusEvent dicttest(StatusMajor::SESSION, StatusMinor::SESS_NEW,
                         "Moar testing is needed");
    GVariant *revparse = dicttest.GetGVariantDict();

    // Reuse the parser in StatusEvent.  As that has already passed the
    // test, expect this to work too.
    StatusEvent cmp(revparse);
    g_variant_unref(revparse);

    ASSERT_EQ(cmp.major, dicttest.major);
    ASSERT_EQ(cmp.minor, dicttest.minor);
    ASSERT_EQ(cmp.message, dicttest.message);
}


std::string test_compare(const StatusEvent& lhs, const StatusEvent& rhs, const bool expect)
{
    bool r = (lhs.major == rhs.major
              && lhs.minor == rhs.minor
              && lhs.message == rhs.message);
    if (r != expect)
    {
        std::stringstream err;
        err << "StatusEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }

    r = (lhs.major != rhs.major
         || lhs.minor != rhs.minor
         || lhs.message != rhs.message);
    if (r == expect)
    {
        std::stringstream err;
        err << "Negative StatusEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }
    return "";
}


TEST(StatusEvent, compare_eq_1)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);
    StatusEvent chk(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);
    std::string res = test_compare(ev, chk, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, compare_eq_2)
{
    StatusEvent ev(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");
    std::string res = test_compare(ev, chk, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, operator_eq_1)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);
    StatusEvent chk(StatusMajor::SESSION, StatusMinor::SESS_AUTH_CHALLENGE);
    ASSERT_TRUE(ev == chk);
}


TEST(StatusEvent, operator_eq_2)
{
    StatusEvent ev(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT, "var1");
    ASSERT_TRUE(ev == chk);
}


TEST(StatusEvent, compare_neq_1)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::CFG_REQUIRE_USER);
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT);
    std::string res = test_compare(ev, chk, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, compare_neq_2)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::PKCS11_DECRYPT, "var1");
    StatusEvent chk(StatusMajor::SESSION, StatusMinor::SESS_BACKEND_COMPLETED,
                    "var1");
    std::string res = test_compare(ev, chk, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, compare_neq_3)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::PKCS11_DECRYPT, "var1");
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT);
    std::string res = test_compare(ev, chk, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(StatusEvent, operator_neq_1)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::CFG_REQUIRE_USER);
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT);
    ASSERT_TRUE(ev != chk);
}


TEST(StatusEvent, operator_neq_2)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::PKCS11_DECRYPT, "var1");
    StatusEvent chk(StatusMajor::SESSION, StatusMinor::SESS_BACKEND_COMPLETED,
                    "var1");
    ASSERT_TRUE(ev != chk);
}


TEST(StatusEvent, operator_neq_3)
{
    StatusEvent ev(StatusMajor::SESSION, StatusMinor::PKCS11_DECRYPT, "var1");
    StatusEvent chk(StatusMajor::PROCESS, StatusMinor::PKCS11_ENCRYPT);
    ASSERT_TRUE(ev != chk);
}


TEST(StatusEvent, stringstream)
{
    StatusEvent status(StatusMajor::CONFIG, StatusMinor::CONN_CONNECTING,
                       "In progress");
#ifdef DEBUG_CORE_EVENTS
    status.show_numeric_status = false; // DEBUG_CORE_EVENTS enables this by default
#endif
    std::stringstream chk;
    chk << status;
    std::string expect("Configuration, Client connecting: In progress");
    ASSERT_EQ(chk.str(), expect);

    std::stringstream chk1;
    status.show_numeric_status = true;
    chk1 << status;
    std::string expect1("[1,6] Configuration, Client connecting: In progress");
    ASSERT_EQ(chk1.str(), expect1);


    StatusEvent status2(StatusMajor::SESSION,
                       StatusMinor::SESS_BACKEND_COMPLETED);
    std::stringstream chk2;
    status2.show_numeric_status = true;
    chk2 << status2;
    std::string expect2("[3,18] Session, Backend Session Object completed");
    ASSERT_EQ(chk2.str(), expect2);

    std::stringstream chk3;
    status2.show_numeric_status = false;
    chk3 << status2;
    std::string expect3("Session, Backend Session Object completed");
    ASSERT_EQ(chk3.str(), expect3);
}


} // namespace unittest
