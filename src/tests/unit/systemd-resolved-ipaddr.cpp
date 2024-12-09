//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024- David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved-ipaddr.cpp
 *
 * @brief  Unit tests for NetCfg::DNS::resolved::IPAddress
 */

#include <gtest/gtest.h>

#include "netcfg/dns/systemd-resolved-ipaddr.hpp"

using namespace NetCfg::DNS::resolved;

namespace unittest {


// clang-format off
namespace TestData {

    /**
    * Correctly formatted IP addresses
    */
    namespace Correct {
        std::string ipv4_str("10.11.12.13");
        std::vector<std::byte> ipv4_byte = {
            std::byte{10}, std::byte{11}, std::byte{12}, std::byte{13}};

        std::string ipv6_str("fd15:53b6:dead::2");
        std::vector<std::byte> ipv6_byte = {
            std::byte{0xf}, std::byte{0xd}, std::byte{0x1}, std::byte{0x5},
            std::byte{0x5}, std::byte{0x3}, std::byte{0xb}, std::byte{0x6},
            std::byte{0xd}, std::byte{0xe}, std::byte{0xa}, std::byte{0xd},
            std::byte{0x0}, std::byte{0x0}, std::byte{0x0}, std::byte{0x2}};
    } // namespace Correct


    /**
    *  IP addresses containing invalid characters
    */
    namespace Invalid {
        std::string ipv4_str("10.1.2.t");
        std::string ipv6_str("fd15:53b6:dead:xcd::2");

        /*
         *  ipv4_byte and ipv6_byte can only contain 8-bit values (0-255)
         *  in each vector element.  That range is valid for IP addresses.
         */
    } // namespace InvalidFormat


    /**
     *   IPv4 in IPv6 addresses
     */
    namespace Mixed {
        std::string ipv4_in_ipv6_str("fd15:53b6:dead::10.2.3.4");
    }


    /**
    *  IP addresses which are truncated
    */
    namespace Truncated {
        std::string ipv4_str("10.1.2");
        std::vector<std::byte> ipv4_byte = {
            std::byte{10}, std::byte{11}, std::byte{13}};

        std::string ipv6_str("fd15:53b6:dead:2");
        std::vector<std::byte> ipv6_byte = {
            std::byte{0xf}, std::byte{0xd}, std::byte{0x1}, std::byte{0x5},
            std::byte{0x5}, std::byte{0x3}, std::byte{0xb}, std::byte{0x6},
            std::byte{0x0}, std::byte{0x0}, std::byte{0x0}, std::byte{0x2}};
    } // namespace Truncated

    /**
     *  IP addresses which exceeds 32 or 128 bits
     */
    namespace TooLong {
        std::string ipv4_str("10.1.2.3.5");
        std::vector<std::byte> ipv4_byte = {
            std::byte{10}, std::byte{11}, std::byte{13}, std::byte{14},
            std::byte{15}};

        std::string ipv6_str("fd15:53b6:dead:0:0:0:0:0::2");
        std::vector<std::byte> ipv6_byte = {
            std::byte{0xf}, std::byte{0xd}, std::byte{0x1}, std::byte{0x5},
            std::byte{0x5}, std::byte{0x3}, std::byte{0xb}, std::byte{0x6},
            std::byte{0xd}, std::byte{0xe}, std::byte{0xa}, std::byte{0xd},
            std::byte{0x0}, std::byte{0x0}, std::byte{0x0}, std::byte{0x1},
            std::byte{0x0}, std::byte{0x0}, std::byte{0x0}, std::byte{0x2},
            std::byte{0x0}, std::byte{0x0}, std::byte{0x0}, std::byte{0x3},
            };
    } // namespace TooLong

    /**
     *  IP addresses which contains group elements exceeding 255 or 0xffff
     */
    namespace Overflow {
        std::string ipv4_str("10.20.423.102");
        std::string ipv6_str("fd15:53b6:dead::ffff0::2");

        /*
         *  ipv4_byte and ipv6_byte can only contain 8-bit values (0-255)
         *  in each vector element.  That range is valid for IP addresses.
         */
    } // namespace Overflow

} // namespace TestData
// clang-format on


/**
 *  Helper function to create a D-Bus value container containing the
 *  IP address family and address.  Data type: (iay)
 *
 * @param family       int32_t with the IP address family value
 * @param addr         std::vector<std::byte> containing the IP address
 * @return GVariant*
 */
static GVariant *create_gvariant_addr(int32_t family, std::vector<std::byte> &addr)
{
    GVariantBuilder *b = glib2::Builder::Create("(iay)");
    glib2::Builder::Add(b, static_cast<int32_t>(family));
    glib2::Builder::Add(b, glib2::Value::CreateVector(addr));
    return glib2::Builder::Finish(b);
}


TEST(resolved_IPAddress, ctor_string_correct)
{
    IPAddress correct_ipv4(TestData::Correct::ipv4_str);
    IPAddress correct_ipv4_override(TestData::Correct::ipv4_str, AF_INET);
    IPAddress correct_ipv6(TestData::Correct::ipv6_str);
    IPAddress correct_ipv6_override(TestData::Correct::ipv6_str, AF_INET6);
}


TEST(resolved_IPAddress, ctor_string_incorrect)
{
    // Incorrectly formatted IP addresses
    EXPECT_THROW(IPAddress err_a(TestData::Truncated::ipv4_str), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_b(TestData::TooLong::ipv4_str), NetCfg::DNS::resolved::Exception);

    EXPECT_THROW(IPAddress err_c(TestData::Truncated::ipv6_str), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_d(TestData::TooLong::ipv6_str), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_e(TestData::Mixed::ipv4_in_ipv6_str), NetCfg::DNS::resolved::Exception);

    EXPECT_THROW(IPAddress err_f(TestData::Overflow::ipv4_str), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_g(TestData::Overflow::ipv6_str), NetCfg::DNS::resolved::Exception);

    // Incorrect IP address family override
    EXPECT_THROW(IPAddress err_h(TestData::Correct::ipv4_str, AF_INET6), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_i(TestData::Correct::ipv6_str, AF_INET), NetCfg::DNS::resolved::Exception);
    EXPECT_THROW(IPAddress err_j(TestData::Correct::ipv6_str, 14), NetCfg::DNS::resolved::Exception);
}


TEST(resolved_IPAddress, ctor_gvariant_correct)
{
    GVariant *ipv4 = create_gvariant_addr(AF_INET, TestData::Correct::ipv4_byte);
    IPAddress parsed_correct_ipv4(ipv4);
    g_variant_unref(ipv4);

    GVariant *ipv6 = create_gvariant_addr(AF_INET6, TestData::Correct::ipv6_byte);
    IPAddress parsed_correct_ipv6(ipv6);
    g_variant_unref(ipv6);
}


TEST(resolved_IPAddress, ctor_gvariant_incorrect)
{
    GVariant *ipv4_short = create_gvariant_addr(AF_INET, TestData::Truncated::ipv4_byte);
    EXPECT_THROW(IPAddress parsed_ipv4_short(ipv4_short), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv4_short);

    GVariant *ipv4_long = create_gvariant_addr(AF_INET, TestData::TooLong::ipv4_byte);
    EXPECT_THROW(IPAddress parsed_ipv4_short(ipv4_long), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv4_long);

    GVariant *ipv6_short = create_gvariant_addr(AF_INET6, TestData::Truncated::ipv6_byte);
    EXPECT_THROW(IPAddress parsed_ipv6_short(ipv6_short), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv6_short);

    GVariant *ipv6_long = create_gvariant_addr(AF_INET6, TestData::TooLong::ipv6_byte);
    EXPECT_THROW(IPAddress parsed_ipv6_long(ipv6_long), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv6_long);

    // Incorrect IP address family
    GVariant *ipv4_inet6 = create_gvariant_addr(AF_INET6, TestData::Correct::ipv4_byte);
    EXPECT_THROW(IPAddress parsed_ipv4_inet6(ipv4_inet6), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv4_inet6);

    GVariant *ipv6_inet = create_gvariant_addr(AF_INET, TestData::Correct::ipv6_byte);
    EXPECT_THROW(IPAddress parsed_ipv6_short(ipv6_inet), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv6_inet);

    GVariant *ipv6_wrong_family = create_gvariant_addr(12, TestData::Correct::ipv6_byte);
    EXPECT_THROW(IPAddress parsed_ipv6_unknown(ipv6_wrong_family), NetCfg::DNS::resolved::Exception);
    g_variant_unref(ipv6_wrong_family);
}


TEST(resolved_IPAddress, method_str)
{
    GVariant *ipv4 = create_gvariant_addr(AF_INET, TestData::Correct::ipv4_byte);
    IPAddress parsed_correct_ipv4(ipv4);
    EXPECT_STREQ(parsed_correct_ipv4.str().c_str(), TestData::Correct::ipv4_str.c_str());
    g_variant_unref(ipv4);

    IPAddress parsed_correct_ipv6(TestData::Correct::ipv6_str);
    EXPECT_STREQ(parsed_correct_ipv6.str().c_str(), "fd15:53b6:dead:0000:0000:0000:0000:0002");
}


TEST(resolved_IPAddress, GetGVariant)
{
    IPAddress correct_ipv4(TestData::Correct::ipv4_str);
    GVariant *ipv4 = correct_ipv4.GetGVariant();
    EXPECT_STREQ(g_variant_get_type_string(ipv4), "(iay)");
    char *buf = g_variant_print(ipv4, true);
    EXPECT_STREQ(buf, "(2, [byte 0x0a, 0x0b, 0x0c, 0x0d])");
    g_variant_unref(ipv4);
    g_free(buf);

    IPAddress correct_ipv6(TestData::Correct::ipv6_str);
    GVariant *ipv6 = correct_ipv6.GetGVariant();
    EXPECT_STREQ(g_variant_get_type_string(ipv6), "(iay)");
    buf = g_variant_print(ipv6, true);
    EXPECT_STREQ(buf, "(10, [byte 0xfd, 0x15, 0x53, 0xb6, 0xde, 0xad, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02])");
    g_variant_unref(ipv6);
    g_free(buf);
}

} // namespace unittest
