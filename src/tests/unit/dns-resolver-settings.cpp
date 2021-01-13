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
 * @file   dns-resolver-settings.cpp
 *
 * @brief  Unit tests for the NetCfg::DNS::ResolverSettings class
 *
 */

#include <iostream>
#include <sstream>
#include <gtest/gtest.h>

#include <openvpn/common/rc.hpp>
#include "dbus/core.hpp"
#include "netcfg/dns/resolver-settings.hpp"

using namespace NetCfg::DNS;

TEST(DNSResolverSettings, init)
{
    ResolverSettings r1(1);
    EXPECT_EQ(r1.GetIndex(), 1);
    EXPECT_EQ(r1.GetNameServers().size(), 0);
    EXPECT_EQ(r1.GetSearchDomains().size(), 0);

    ResolverSettings::Ptr r2 = new ResolverSettings(2);
    EXPECT_EQ(r2->GetIndex(), 2);
    EXPECT_EQ(r2->GetNameServers().size(), 0);
    EXPECT_EQ(r2->GetSearchDomains().size(), 0);
}


TEST(DNSResolverSettings, enabled_flag)
{
    ResolverSettings r1(1);
    EXPECT_EQ(r1.GetEnabled(), false);
    r1.Enable();
    EXPECT_EQ(r1.GetEnabled(), true);
    r1.Disable();
    EXPECT_EQ(r1.GetEnabled(), false);

    ResolverSettings::Ptr r2 = new ResolverSettings(2);
    EXPECT_EQ(r2->GetEnabled(), false);
    r2->Enable();
    EXPECT_EQ(r2->GetEnabled(), true);
    r2->Disable();
    EXPECT_EQ(r2->GetEnabled(), false);
}


TEST(DNSResolverSettings, AddNameServer)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(3);
    r1->AddNameServer("1.2.3.4");

    std::vector<std::string> chk = r1->GetNameServers();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);
    EXPECT_STREQ(chk[0].c_str(), "1.2.3.4");
}

TEST(DNSResolverSettings, AddSearchDomain)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(4);
    r1->AddSearchDomain("test.example.org");

    EXPECT_EQ(r1->GetNameServers().size(), 0);

    std::vector<std::string> chk = r1->GetSearchDomains();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_STREQ(chk[0].c_str(), "test.example.org");
}

TEST(DNSResolverSettings, MultipleEntries)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(5);
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(10 + i) << "." << std::to_string(11 + i) << "."
           << std::to_string(12 + i) << "." << std::to_string(13 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i+1) << ".example.net";
        r1->AddSearchDomain(sd.str());
    }

    // Check the contents of these entries
    std::vector<std::string> nslist = r1->GetNameServers();
    std::vector<std::string> sdlist = r1->GetSearchDomains();
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(10 + i) << "." << std::to_string(11 + i) << "."
           << std::to_string(12 + i) << "." << std::to_string(13 + i);
        EXPECT_STREQ(nslist[i].c_str(), ns.str().c_str());

        std::stringstream sd;
        sd << "test" << std::to_string(i+1) << ".example.net";
        EXPECT_STREQ(sdlist[i].c_str(), sd.str().c_str());
    }
}


TEST(DNSResolverSettings, DuplicatedEntries)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(12);
    r1->AddNameServer("9.9.9.9");
    ASSERT_EQ(r1->GetNameServers().size(), 1);
    r1->AddNameServer("8.7.6.5");
    ASSERT_EQ(r1->GetNameServers().size(), 2);
    r1->AddNameServer("9.9.9.9");
    ASSERT_EQ(r1->GetNameServers().size(), 2);

    ASSERT_EQ(r1->GetSearchDomains().size(), 0);
    r1->AddSearchDomain("test1.example.site");
    ASSERT_EQ(r1->GetSearchDomains().size(), 1);
    r1->AddSearchDomain("test2.example.site");
    r1->AddSearchDomain("test3.example.site");
    ASSERT_EQ(r1->GetSearchDomains().size(), 3);
    r1->AddSearchDomain("test2.example.site");
    ASSERT_EQ(r1->GetSearchDomains().size(), 3);
}


TEST(DNSResolverSettings, ClearEntries)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(6);
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(20 + i) << "." << std::to_string(21 + i) << "."
           << std::to_string(22 + i) << "." << std::to_string(23 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i+1) << ".example.com";
        r1->AddSearchDomain(sd.str());
    }

    // Sanity checks
    EXPECT_EQ(r1->GetIndex(), 6);
    EXPECT_EQ(r1->GetNameServers().size(), 5);
    EXPECT_EQ(r1->GetSearchDomains().size(), 5);

    r1->ClearNameServers();
    EXPECT_EQ(r1->GetNameServers().size(), 0);

    r1->ClearSearchDomains();
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);
}

TEST(DNSResolverSettings, string)
{
    ResolverSettings r1(7);
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(30 + i) << "." << std::to_string(31 + i) << "."
           << std::to_string(32 + i) << "." << std::to_string(33 + i);
        r1.AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i+1) << ".example.community";
        r1.AddSearchDomain(sd.str());
    }

    std::stringstream chk1;
    chk1 << r1;
    EXPECT_STREQ(chk1.str().c_str(), "(Settings not enabled)");

    r1.Enable();
    std::stringstream chk2;
    chk2 << r1;
    EXPECT_STREQ(chk2.str().c_str(),
                 "DNS resolvers: 30.31.32.33, 31.32.33.34, 32.33.34.35, "
                 "33.34.35.36, 34.35.36.37 "
                 "- Search domains: test1.example.community, "
                 "test2.example.community, test3.example.community, "
                 "test4.example.community, test5.example.community");
}

TEST(DNSResolverSettings, string_Ptr)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(8);
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(40 + i) << "." << std::to_string(41 + i) << "."
           << std::to_string(42 + i) << "." << std::to_string(43 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i+10) << ".example.community";
        r1->AddSearchDomain(sd.str());
    }

    std::stringstream chk1;
    chk1 << r1;
    EXPECT_STREQ(chk1.str().c_str(), "(Settings not enabled)");

    r1->Enable();
    std::stringstream chk2;
    chk2 << r1;
    EXPECT_STREQ(chk2.str().c_str(),
                 "DNS resolvers: 40.41.42.43, 41.42.43.44, 42.43.44.45, "
                 "43.44.45.46, 44.45.46.47 "
                 "- Search domains: test10.example.community, "
                 "test11.example.community, test12.example.community, "
                 "test13.example.community, test14.example.community");
}

TEST(DNSResolverSettings, GVariantTests_SingleNameServer)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(9);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"9.9.9.9"}};
    GVariant* d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(ns));
    std::string res = r1->AddNameServers(d);
    ASSERT_STREQ(res.c_str(), "9.9.9.9");
    g_variant_unref(d);

    std::vector<std::string> chk = r1->GetNameServers();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);
    ASSERT_STREQ(chk[0].c_str(), "9.9.9.9");

    r1->Enable();
    std::stringstream chk_str;
    chk_str << r1;
    ASSERT_STREQ(chk_str.str().c_str(), "DNS resolvers: 9.9.9.9");
}


TEST(DNSResolverSettings, GVariantTests_SingleSearchDomain)
{
    ResolverSettings r1(10);

    // Insert a single name server and search domain
    std::vector<std::string> sd = {{"sub0.example.net"}};
    GVariant* d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(sd));
    r1.AddSearchDomains(d);
    g_variant_unref(d);

    std::vector<std::string> chk = r1.GetSearchDomains();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_EQ(r1.GetNameServers().size(), 0);
    ASSERT_STREQ(chk[0].c_str(), "sub0.example.net");

    r1.Enable();
    std::stringstream chk_str;
    chk_str << r1;
    ASSERT_STREQ(chk_str.str().c_str(), "Search domains: sub0.example.net");
}


TEST(DNSResolverSettings, GVariantTests_MultipleEntries)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(11);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"10.0.0.1", "10.0.2.2", "10.0.3.3"}};
    GVariant* d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(ns));
    std::string res = r1->AddNameServers(d);
    ASSERT_STREQ(res.c_str(), "10.0.0.1, 10.0.2.2, 10.0.3.3");
    g_variant_unref(d);

    std::vector<std::string> sd = {{"sub1.example.net", "sub2.example.com",
                                    "sub3.example.org", "sub4.test.example"}};
    d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(sd));
    r1->AddSearchDomains(d);
    g_variant_unref(d);

    std::vector<std::string> chk_ns = r1->GetNameServers();
    ASSERT_EQ(chk_ns.size(), 3);

    std::vector<std::string> chk_sd = r1->GetSearchDomains();
    ASSERT_EQ(chk_sd.size(), 4);

    r1->Enable();
    std::stringstream chk_str;
    chk_str << r1;
    ASSERT_STREQ(chk_str.str().c_str(),
                 "DNS resolvers: 10.0.0.1, 10.0.2.2, 10.0.3.3 - "
                 "Search domains: sub1.example.net, sub2.example.com, "
                 "sub3.example.org, sub4.test.example");
}


TEST(DNSResolverSettings, GVariantTests_DuplicatedEntries)
{
    ResolverSettings::Ptr r1 = new ResolverSettings(13);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"10.0.0.1", "10.0.0.2", "10.0.0.2"}};
    GVariant* d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(ns));
    r1->AddNameServers(d);
    g_variant_unref(d);
    ASSERT_EQ(r1->GetNameServers().size(), 2);

    std::vector<std::string> sd = {{"sub1.example.net", "sub2.example.com",
                                    "sub1.example.net", "sub2.example.com"}};
    d = GLibUtils::wrapInTuple(GLibUtils::GVariantBuilderFromVector(sd));
    r1->AddSearchDomains(d);
    g_variant_unref(d);
    ASSERT_EQ(r1->GetSearchDomains().size(), 2);
}
