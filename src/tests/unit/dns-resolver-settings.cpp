//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   dns-resolver-settings.cpp
 *
 * @brief  Unit tests for the NetCfg::DNS::ResolverSettings class
 *
 */

#include <iostream>
#include <sstream>
#include <gdbuspp/glib2/utils.hpp>
#include <glib.h>

#include <gtest/gtest.h>

#include "netcfg/dns/resolver-settings.hpp"

using namespace NetCfg::DNS;

TEST(DNSResolverSettings, init)
{
    auto r1 = ResolverSettings::Create(1);
    EXPECT_EQ(r1->GetIndex(), 1);
    EXPECT_EQ(r1->GetNameServers().size(), 0);
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);

    auto r2 = ResolverSettings::Create(2);
    EXPECT_EQ(r2->GetIndex(), 2);
    EXPECT_EQ(r2->GetNameServers().size(), 0);
    EXPECT_EQ(r2->GetSearchDomains().size(), 0);
}


TEST(DNSResolverSettings, enabled_flag)
{
    auto r1 = ResolverSettings::Create(1);

    EXPECT_EQ(r1->GetEnabled(), false);
    r1->Enable();
    EXPECT_EQ(r1->GetEnabled(), true);
    r1->Disable();
    EXPECT_EQ(r1->GetEnabled(), false);

    auto r2 = ResolverSettings::Create(2);
    EXPECT_EQ(r2->GetEnabled(), false);
    r2->Enable();
    EXPECT_EQ(r2->GetEnabled(), true);
    r2->Disable();
    EXPECT_EQ(r2->GetEnabled(), false);
}


TEST(DNSResolverSettings, AddNameServer)
{
    auto r1 = ResolverSettings::Create(3);

    r1->AddNameServer("1.2.3.4");

    std::vector<std::string> chk = r1->GetNameServers();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);
    EXPECT_STREQ(chk[0].c_str(), "1.2.3.4");
}


TEST(DNSResolverSettings, AddNameServer_multiple)
{
    auto r1 = ResolverSettings::Create(14);

    std::vector<std::string> list = {
        "1.1.1.1",
        "2.2.2.2",
        "3.3.3.3"};

    for (const auto &s : list)
    {
        r1->AddNameServer(s);
    }

    std::vector<std::string> chk = r1->GetNameServers();
    ASSERT_EQ(chk.size(), 3);
    EXPECT_EQ(r1->GetSearchDomains().size(), 0);

    for (unsigned int i = 0; i < chk.size(); i++)
    {
        EXPECT_STREQ(chk[i].c_str(), list[i].c_str());
    }
}


TEST(DNSResolverSettings, AddSearchDomain)
{
    auto r1 = ResolverSettings::Create(4);

    r1->AddSearchDomain("test.example.org");

    EXPECT_EQ(r1->GetNameServers().size(), 0);

    std::vector<std::string> chk = r1->GetSearchDomains();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_STREQ(chk[0].c_str(), "test.example.org");
}

TEST(DNSResolverSettings, MultipleEntries)
{
    auto r1 = ResolverSettings::Create(5);

    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(10 + i) << "." << std::to_string(11 + i) << "."
           << std::to_string(12 + i) << "." << std::to_string(13 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i + 1) << ".example.net";
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
        sd << "test" << std::to_string(i + 1) << ".example.net";
        EXPECT_STREQ(sdlist[i].c_str(), sd.str().c_str());
    }
}


TEST(DNSResolverSettings, DuplicatedEntries)
{
    auto r1 = ResolverSettings::Create(12);

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
    auto r1 = ResolverSettings::Create(6);

    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(20 + i) << "." << std::to_string(21 + i) << "."
           << std::to_string(22 + i) << "." << std::to_string(23 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i + 1) << ".example.com";
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


TEST(DNSResolverSettings, no_settings_str)
{
    auto r1 = ResolverSettings::Create(7);

    std::stringstream chk1;
    chk1 << r1;
    EXPECT_STREQ(chk1.str().c_str(), "(No DNS resolver settings)");

    r1->Enable();
    std::stringstream chk2;
    chk2 << r1;
    EXPECT_STREQ(chk2.str().c_str(), "(No DNS resolver settings)");
}


TEST(DNSResolverSettings, only_search_str)
{
    auto r1 = ResolverSettings::Create(7);
    for (int i = 0; i < 5; ++i)
    {
        std::stringstream sd;
        sd << "test" << std::to_string(i + 1) << ".example.com";
        r1->AddSearchDomain(sd.str());
    }
    std::stringstream chk1;
    chk1 << r1;
    EXPECT_STREQ(chk1.str().c_str(), "(Settings not enabled)");

    r1->Enable();
    std::stringstream chk2;
    chk2 << r1;
    EXPECT_STREQ(chk2.str().c_str(),
                 "Search domains: test1.example.com, test2.example.com, "
                 "test3.example.com, test4.example.com, test5.example.com");
}


TEST(DNSResolverSettings, string_Ptr)
{
    auto r1 = ResolverSettings::Create(8);

    for (int i = 0; i < 5; ++i)
    {
        std::stringstream ns;
        ns << std::to_string(40 + i) << "." << std::to_string(41 + i) << "."
           << std::to_string(42 + i) << "." << std::to_string(43 + i);
        r1->AddNameServer(ns.str());

        std::stringstream sd;
        sd << "test" << std::to_string(i + 10) << ".example.community";
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
    auto r1 = ResolverSettings::Create(9);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"9.9.9.9"}};
    GVariant *d = glib2::Value::CreateTupleWrapped(ns);
    std::string res = r1->AddNameServers(d);
    ASSERT_STREQ(res.c_str(), "9.9.9.9");

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
    auto r1 = ResolverSettings::Create(10);

    // Insert a single name server and search domain
    std::vector<std::string> sd = {{"sub0.example.net"}};
    GVariant *d = glib2::Value::CreateTupleWrapped(sd);
    r1->AddSearchDomains(d);

    std::vector<std::string> chk = r1->GetSearchDomains();
    ASSERT_EQ(chk.size(), 1);
    EXPECT_EQ(r1->GetNameServers().size(), 0);
    ASSERT_STREQ(chk[0].c_str(), "sub0.example.net");

    r1->Enable();
    std::stringstream chk_str;
    chk_str << r1;
    ASSERT_STREQ(chk_str.str().c_str(), "Search domains: sub0.example.net");
}


TEST(DNSResolverSettings, GVariantTests_MultipleEntries)
{
    auto r1 = ResolverSettings::Create(11);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"10.0.0.1", "10.0.2.2", "10.0.3.3"}};
    GVariant *d = glib2::Value::CreateTupleWrapped(ns);
    std::string res = r1->AddNameServers(d);
    ASSERT_STREQ(res.c_str(), "10.0.0.1, 10.0.2.2, 10.0.3.3");

    std::vector<std::string> sd = {{"sub1.example.net", "sub2.example.com", "sub3.example.org", "sub4.test.example"}};
    d = glib2::Value::CreateTupleWrapped(sd);
    r1->AddSearchDomains(d);

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
    auto r1 = ResolverSettings::Create(13);

    // Insert a single name server and search domain
    std::vector<std::string> ns = {{"10.0.0.1", "10.0.0.2", "10.0.0.2"}};
    GVariant *d = glib2::Value::CreateTupleWrapped(ns);
    r1->AddNameServers(d);
    ASSERT_EQ(r1->GetNameServers().size(), 2);

    std::vector<std::string> sd = {
        {"sub1.example.net", "sub2.example.com", "sub1.example.net", "sub2.example.com"}};
    d = glib2::Value::CreateTupleWrapped(sd);
    r1->AddSearchDomains(d);
    ASSERT_EQ(r1->GetSearchDomains().size(), 2);
}
