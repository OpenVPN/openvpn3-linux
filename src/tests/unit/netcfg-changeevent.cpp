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
 * @file   netcfg-changeeevent.cpp
 *
 * @brief  Unit test for struct NetCfgChangeEvent
 */

#include <iostream>
#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "dbus/core.hpp"
#include "netcfg/netcfg-changeevent.hpp"

namespace unittest {

std::string test_empty(const NetCfgChangeEvent& ev, const bool expect)
{
    bool r = ev.empty();

    if (expect != r)
    {
        return std::string("test_empty():  ")
               + "ev.empty() = " + (r ? "true" : "false")
               + " [expected: " + (expect ? "true" : "false") + "]";
    }

    r = (NetCfgChangeType::UNSET == ev.type
         && ev.device.empty()
         && ev.details.empty());
    if (expect != r)
    {
        return std::string("test_empty() - Member check:  ")
               + "(" + std::to_string((unsigned) ev.type) + ", "
               + "'" + ev.device + "', "
               + "details.size=" + std::to_string(ev.details.size()) + ") ..."
               + " is " + (r ? "EMPTY" : "NON-EMPTY")
               + " [expected: " + (expect ? "EMPTY" : "NON-EMPTY") + "]";
    }
    return "";
};


TEST(NetCfgChangeEvent, init_empty)
{
    NetCfgChangeEvent empty;
    std::string res = test_empty(empty, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, init_with_values)
{
    NetCfgChangeEvent populated1(NetCfgChangeType::DEVICE_ADDED, "test-dev",
                                 {{"some_key", "Some detail"}});
    std::string res = test_empty(populated1, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, reset)
{
    NetCfgChangeEvent populated1(NetCfgChangeType::DEVICE_ADDED, "test-dev",
                                 {{"some_key", "Some detail"}});
    populated1.reset();
    std::string res = test_empty(populated1, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, stringstream)
{
    NetCfgChangeEvent event(NetCfgChangeType::IPADDR_ADDED, "testdev",
                            {{"ip_address", "2001:db8:a050::1"},
                             {"prefix", "64"}});
    std::stringstream chk;
    chk << event;
    std::string expect("Device testdev - IP Address Added: ip_address='2001:db8:a050::1', prefix='64'");

    ASSERT_TRUE(chk.str() == expect);
}


TEST(NetCfgChangeEvent, gvariant_GetVariant)
{
    NetCfgChangeEvent g_state(NetCfgChangeType::ROUTE_ADDED, "tun22",
                              {{"ip_address", "2001:db8:a050::1"},
                               {"prefix", "64"}});
    GVariant *chk = g_state.GetGVariant();
    gchar *dmp = g_variant_print(chk, true);
    std::string dump_check(dmp);
    g_free(dmp);
    g_variant_unref(chk);

    std::string expect = "(uint32 16, 'tun22', {'ip_address': '2001:db8:a050::1', 'prefix': '64'})";
    ASSERT_EQ(dump_check, expect);
}


TEST(NetCfgChangeEvent, gvariant_get)
{
    NetCfgChangeEvent g_state(NetCfgChangeType::ROUTE_ADDED, "tun22",
                              {{"ip_address", "2001:db8:a050::1"},
                               {"prefix", "64"}});
    GVariant *chk = g_state.GetGVariant();

    guint type = 0;
    gchar *dev_s = nullptr;
    GVariantIter *det_g = nullptr;
    g_variant_get(chk, "(usa{ss})", &type, &dev_s, &det_g);
    g_variant_unref(chk);

    NetCfgChangeDetails det_s;
    GVariant *kv = nullptr;
    while ((kv = g_variant_iter_next_value(det_g)))
    {
        gchar *key = nullptr;
        gchar *value = nullptr;
        g_variant_get(kv, "{ss}", &key, &value);

        det_s[key] = std::string(value);
        g_free(key);
        g_free(value);
        g_variant_unref(kv);
    }
    g_variant_iter_free(det_g);

    ASSERT_EQ(type, (guint) g_state.type);
    ASSERT_EQ(dev_s, g_state.device);
    ASSERT_EQ(det_s, g_state.details);
    g_free(dev_s);
}


TEST(NetCfgChangeEvent, parse_gvariant_valid_data)
{
    NetCfgChangeEvent event(NetCfgChangeType::ROUTE_ADDED, "tun33",
                            {{"ip_address", "2001:db8:a050::1"},
                             {"prefix", "64"}});

    //
    // Build up an GVariant object similar to what we would retrieve
    // in the various GLib2/GDBus calls when receiving this kind of event
    //
    // All values below here must match the values as provided in event
    //
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("(usa{ss})"));
    g_variant_builder_add(b, "u", (guint32) NetCfgChangeType::ROUTE_ADDED);
    g_variant_builder_add(b, "s", "tun33");

    // Add the details - key/value dictionary
    g_variant_builder_open(b, G_VARIANT_TYPE ("a{ss}"));

    // Add IP address
    g_variant_builder_open(b, G_VARIANT_TYPE("{ss}"));
    g_variant_builder_add(b, "s", "ip_address");
    g_variant_builder_add(b, "s", "2001:db8:a050::1");
    g_variant_builder_close(b);

    // Add prefix
    g_variant_builder_open(b, G_VARIANT_TYPE("{ss}"));
    g_variant_builder_add(b, "s", "prefix");
    g_variant_builder_add(b, "s", "64");
    g_variant_builder_close(b);

    // Details ready
    g_variant_builder_close(b);

    // Retrieve the proper GVariant object
    GVariant *chk = g_variant_builder_end(b);
    g_variant_builder_clear(b);
    g_variant_builder_unref(b);

    NetCfgChangeEvent parsed(chk);
    g_variant_unref(chk);
    ASSERT_EQ(parsed, event);
}


TEST(NetCfgChangeEvent, parse_gvariant_invalid_data)
{
    GVariant *invalid = g_variant_new("(uus)", 123, 456, "Invalid data");
    ASSERT_THROW(NetCfgChangeEvent invalid_event(invalid),
                 NetCfgException);
    g_variant_unref(invalid);
}

std::string test_compare(const NetCfgChangeEvent& lhs,
                         const NetCfgChangeEvent& rhs, const bool expect)
{
    bool r = (lhs.type == rhs.type
              && lhs.device == rhs.device
              && lhs.details== rhs.details);
    if (r != expect)
    {
        std::stringstream err;
        err << "NetCfgChangeEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }

    r = (lhs.type != rhs.type
         || lhs.device != rhs.device
         || lhs.details != rhs.details);
    if (r == expect)
    {
        std::stringstream err;
        err << "Negative NetCfgChangeEvent compare check FAIL: "
            << "{" << lhs << "} == {" << rhs << "} returned "
            << ( r ? "true": "false")
            << " - expected: "
            << (expect ? "true": "false");
        return err.str();
    }
    return "";
}


TEST(NetCfgChangeEvent, compare_eq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    std::string res = test_compare(s1, s2, true);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, operator_eq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    ASSERT_TRUE(s1 == s2);
}

TEST(NetCfgChangeEvent, compare_neq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::DNS_SERVER_REMOVED, "tun11",
                         {{"dns_server", "127.0.0.1"}});
    std::string res = test_compare(s1, s2, false);
    ASSERT_TRUE(res.empty()) << res;
}


TEST(NetCfgChangeEvent, operator_neq)
{
    NetCfgChangeEvent s1(NetCfgChangeType::ROUTE_ADDED, "tun22",
                         {{"ip_address", "2001:db8:a050::1"},
                          {"prefix", "64"}});
    NetCfgChangeEvent s2(NetCfgChangeType::DNS_SERVER_REMOVED, "tun11",
                         {{"dns_server", "127.0.0.1"}});
    ASSERT_TRUE(s1 != s2);
}


TEST(NetCfgChangeEvent, FilterMaskList)
{
    uint16_t mask = (1 << 16) - 1;
    std::vector<std::string> all_set = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(all_set.size(), 16) << "Unexpected size of return vector";

    mask = 2 | 16 | 2048 | 8192;
    std::vector<std::string> selection = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(selection.size(), 4) << "Expected only 4 elements to be returned";

    mask = 0;
    std::vector<std::string> none = NetCfgChangeEvent::FilterMaskList(mask, true);
    ASSERT_EQ(none.size(), 0) << "Expected no elements to be returned";
}


} // namespace unittest
