//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   sessionmgr-events.cpp
 *
 * @brief  Unit tests for SessionManager::Event
 */


#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "dbus/core.hpp"
#include "sessionmgr/sessionmgr-events.hpp"

using namespace SessionManager;

namespace unittest {

TEST(SessionManagerEvent, init_empty)
{
    Event empty1;
    ASSERT_TRUE(empty1.empty()) << "Not an empty object without init";

    Event empty2{};
    ASSERT_TRUE(empty2.empty()) << "Not an empty object with {} init";
}


TEST(SessionManagerEvent, init_with_values)
{
    Event ev1{"/net/openvpn/v3/test/1", EventType::UNSET, 123};
    ASSERT_FALSE(ev1.empty()) << "Not initialized properly with EventType::UNSET";
    ASSERT_EQ(ev1.path, std::string("/net/openvpn/v3/test/1")) << "Invalid path";
    ASSERT_EQ(ev1.type, EventType::UNSET);
    ASSERT_EQ(ev1.owner, 123);
    ASSERT_EQ(Event::TypeStr(ev1.type), std::string("[UNSET]"));

    std::stringstream chk1;
    chk1 << ev1;
    std::string expect1("[UNSET]; owner: 123, path: /net/openvpn/v3/test/1");
    ASSERT_EQ(chk1.str(), expect1);


    Event ev2{"/net/openvpn/v3/test/2", EventType::SESS_CREATED, 234};
    ASSERT_FALSE(ev2.empty()) << "Not initialized properly with EventType::SESS_CREATED";
    ASSERT_EQ(ev2.path, std::string("/net/openvpn/v3/test/2")) << "Invalid path";
    ASSERT_EQ(ev2.type, EventType::SESS_CREATED);
    ASSERT_EQ(ev2.owner, 234);
    ASSERT_EQ(Event::TypeStr(ev2.type), std::string("Session created"));
    ASSERT_EQ(Event::TypeStr(ev2.type, true), "SESS_CREATED");

    std::stringstream chk2;
    chk2 << ev2;
    std::string expect2("Session created; owner: 234, path: /net/openvpn/v3/test/2");
    ASSERT_EQ(chk2.str(), expect2);

    Event ev3{"/net/openvpn/v3/test/3", EventType::SESS_DESTROYED, 456};
    ASSERT_FALSE(ev3.empty()) << "Not initialized properly with EventType::SESS_DESTROYED";
    ASSERT_EQ(ev3.path, std::string("/net/openvpn/v3/test/3")) << "Invalid path";
    ASSERT_EQ(ev3.type, EventType::SESS_DESTROYED);
    ASSERT_EQ(ev3.owner, 456);
    ASSERT_EQ(Event::TypeStr(ev3.type), std::string("Session destroyed"));
    ASSERT_EQ(Event::TypeStr(ev3.type, true), "SESS_DESTROYED");

    std::stringstream chk3;
    chk3 << ev3;
    std::string expect3("Session destroyed; owner: 456, path: /net/openvpn/v3/test/3");
    ASSERT_EQ(chk3.str(), expect3);
}

TEST(SessionManagerEvent, init_with_gvariant_valid)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("(oqu)"));
    g_variant_builder_add(b, "o", "/net/openvpn/v3/test/4");
    g_variant_builder_add(b, "q", 1);
    g_variant_builder_add(b, "u", 567);
    GVariant *data = g_variant_builder_end(b);
    g_variant_builder_clear(b);
    g_variant_builder_unref(b);

    Event ev(data);
    Event chk{"/net/openvpn/v3/test/4", EventType::SESS_CREATED, 567};
    ASSERT_EQ(ev, chk) << "Not an equal result with valid gvariant data";
    g_variant_unref(data);
}


TEST(SessionManagerEvent, init_with_gvariant_invalid)
{
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("(si)"));
    g_variant_builder_add(b, "s", "/net/openvpn/v3/test/5");
    g_variant_builder_add(b, "i", 5);
    GVariant *data = g_variant_builder_end(b);
    g_variant_builder_clear(b);
    g_variant_builder_unref(b);
    ASSERT_THROW(Event ev(data), SessionManager::Exception);
    g_variant_unref(data);

    b = g_variant_builder_new(G_VARIANT_TYPE("(oqu)"));
    g_variant_builder_add(b, "o", "/net/openvpn/v3/test/6");
    g_variant_builder_add(b, "q", 6);
    g_variant_builder_add(b, "u", 678);
    data = g_variant_builder_end(b);
    g_variant_builder_clear(b);
    g_variant_builder_unref(b);
    ASSERT_THROW(Event ev(data), SessionManager::Exception);
    g_variant_unref(data);

}

TEST(SessionManagerEvent, gvariant_get)
{
    Event ev{"/net/openvpn/v3/test/7", EventType::SESS_DESTROYED, 789};
    GVariant* chk = ev.GetGVariant();
    gchar *dmp = g_variant_print(chk, true);
    std::string dump_check(dmp);
    g_free(dmp);
    g_variant_unref(chk);

    std::string expect = "(objectpath '/net/openvpn/v3/test/7', uint16 2, uint32 789)";
    ASSERT_EQ(dump_check, expect);
}


} // namespace unittests
