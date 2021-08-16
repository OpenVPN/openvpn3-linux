//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
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
 * @file   dns-settings-manager-test.cpp
 *
 * @brief  [enter description of what this file contains]
 */

#include <algorithm>
#include <iostream>
#include <vector>
#include <gtest/gtest.h>

#include <openvpn/common/rc.hpp>
#include "netcfg/dns/resolver-backend-interface.hpp"
#include "netcfg/dns/settings-manager.hpp"

using namespace NetCfg::DNS;

class TestBackend : public ResolverBackendInterface
{
public:
    typedef RCPtr<TestBackend> Ptr;

    TestBackend() = default;
    ~TestBackend() = default;

    const std::string GetBackendInfo() const noexcept override
    {
        return "TestBackend: DNS configuration tester";
    }

    const ApplySettingsMode GetApplyMode() const noexcept override
    {
        return ApplySettingsMode::MODE_PRE;  // Not relevant in this test
    }

    const char *ServerList() const
    {
        return server_list.c_str();
    }

    const char *DomainList() const
    {
        return domain_list.c_str();
    }

    // Save the list of servers and domains in these string variables
    // to be inspected by the unit tests
    std::string server_list;
    std::string domain_list;

protected:
    void Apply(const ResolverSettings::Ptr settings) override
    {
        if (!settings->GetEnabled())
        {
            return;
        }

        std::vector<std::string> srvs = settings->GetNameServers();
        servers.insert(servers.end(), srvs.begin(), srvs.end());

        std::vector<std::string> dmns = settings->GetSearchDomains();
        domains.insert(domains.end(), dmns.begin(), dmns.end());
    }

    void Commit(NetCfgSignals *not_used) override
    {
        std::stringstream srv;

        bool first = true;
        for (const auto& s : servers)
        {
            srv << (!first ? ", " : "") << s;
            first = false;
        }
        servers.clear();
        server_list = srv.str();

        std::stringstream dom;
        first = true;
        for (const auto& d : domains)
        {
            dom << (!first ? ", " : "") << d;
            first = false;
        }
        domains.clear();
        domain_list = dom.str();
    }


private:
    std::vector<std::string> servers;
    std::vector<std::string> domains;
};

TestBackend::Ptr test_backend = nullptr;

class DNSSettingsManager_SingleSetup : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_backend = new TestBackend();
        dnsmgr = new SettingsManager(test_backend);
    }

public:
    SettingsManager::Ptr dnsmgr = nullptr;
};


TEST_F(DNSSettingsManager_SingleSetup, BackendInfo)
{
    // Sanity checks
    std::string expect = std::string("TestBackend: DNS configuration tester");
    std::string be = test_backend->GetBackendInfo();
    ASSERT_EQ(be, expect);

    std::string mgr(dnsmgr->GetBackendInfo());
    ASSERT_STREQ(mgr.c_str(), be.c_str());
}


TEST_F(DNSSettingsManager_SingleSetup, SingleServerDomain)
{
    ResolverSettings::Ptr con1 = dnsmgr->NewResolverSettings();
    con1->AddNameServer("1.1.1.1");
    con1->AddSearchDomain("con1.example.org");
    con1->Enable();
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con1.example.org");

    con1->PrepareRemoval();
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "");
    ASSERT_STREQ(test_backend->DomainList(), "");
}

TEST_F(DNSSettingsManager_SingleSetup, DoubleServerDomain)
{
    ResolverSettings::Ptr con1 = dnsmgr->NewResolverSettings();
    con1->AddNameServer("1.1.1.1");
    con1->AddNameServer("1.1.2.2");
    con1->AddSearchDomain("con1.example.org");
    con1->AddSearchDomain("con1b.example.org");
    con1->Enable();
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "1.1.1.1, 1.1.2.2");
    ASSERT_STREQ(test_backend->DomainList(), "con1.example.org, con1b.example.org");

    con1->PrepareRemoval();
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "");
    ASSERT_STREQ(test_backend->DomainList(), "");
}

TEST_F(DNSSettingsManager_SingleSetup, PrepareRemoval)
{
    ResolverSettings::Ptr con1 = dnsmgr->NewResolverSettings();
    con1->AddNameServer("1.2.3.4");
    con1->AddNameServer("5.6.7.8");
    con1->AddSearchDomain("prepremoval.example.org");
    con1->Enable();
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "1.2.3.4, 5.6.7.8");
    ASSERT_STREQ(test_backend->DomainList(), "prepremoval.example.org");

    con1->PrepareRemoval();
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "");
    ASSERT_STREQ(test_backend->DomainList(), "");

    std::vector<std::string> chk = con1->GetNameServers();
    ASSERT_EQ(chk.size(), 0);
    chk = con1->GetNameServers(true);
    ASSERT_EQ(chk.size(), 2);
    chk = con1->GetNameServers(false);
    ASSERT_EQ(chk.size(), 0);

    chk = con1->GetSearchDomains();
    ASSERT_EQ(chk.size(), 0);
    chk = con1->GetSearchDomains(true);
    ASSERT_EQ(chk.size(), 1);
    chk = con1->GetSearchDomains(false);
    ASSERT_EQ(chk.size(), 0);
}


class DNSSettingsManager_MultipleSessions : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_backend = new TestBackend();
        dnsmgr = new SettingsManager(test_backend);
    }

    void TearDown() override
    {
        for (const auto& c : cfgs)
        {
            c->PrepareRemoval();
        }
        dnsmgr->ApplySettings(nullptr);
    }

    void Configure(unsigned int num, unsigned int numsrv = 1,
                   unsigned int start = 1)
    {
        for (unsigned int i = start; i < start+num; i++)
        {
            ResolverSettings::Ptr rs = dnsmgr->NewResolverSettings();
            for (unsigned int j = 0; j < numsrv; j++)
            {
                rs->AddNameServer(gen_ip(i, i+j));
            }
            rs->AddSearchDomain(gen_search(i));
            rs->Enable();
            cfgs.push_back(rs);
        }
    }

    void RemoveConfig(unsigned int num)
    {
        for (const auto& c : cfgs)
        {
            if (c && c->GetIndex() == num)
            {
                c->PrepareRemoval();
                cfgs.erase(std::remove(cfgs.begin(),
                                       cfgs.end(), c),
                                       cfgs.end());
            }
        }
    }

private:
    std::string gen_ip(unsigned int i, unsigned int j)
    {
        std::stringstream out;
        out << std::to_string(i) << "." << std::to_string(i) << "."
            << std::to_string(j) << "." << std::to_string(j);
        return out.str();
    }

    std::string gen_search(unsigned int i)
    {
        return std::string("con") + std::to_string(i) + ".example.org";
    }

public:
    SettingsManager::Ptr dnsmgr = nullptr;
    std::vector<ResolverSettings::Ptr> cfgs;

};


TEST_F(DNSSettingsManager_MultipleSessions, TwoEntires)
{
    Configure(2);
    dnsmgr->ApplySettings(nullptr);

    // ResolverSettings are processed in the reverse order
    ASSERT_STREQ(test_backend->ServerList(), "2.2.2.2, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con2.example.org, con1.example.org");
}

TEST_F(DNSSettingsManager_MultipleSessions, TwoEntires_DelFirst)
{
    Configure(2);
    dnsmgr->ApplySettings(nullptr);

    // Delete the first (1.1.1.1) settings
    RemoveConfig(0);
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "2.2.2.2");
    ASSERT_STREQ(test_backend->DomainList(), "con2.example.org");
}


TEST_F(DNSSettingsManager_MultipleSessions, TwoEntires_DelLast)
{
    Configure(2);
    dnsmgr->ApplySettings(nullptr);

    // Delete the last (2.2.2.2) settings
    RemoveConfig(1);
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con1.example.org");
}


TEST_F(DNSSettingsManager_MultipleSessions, ThreeEntires_DelMiddle)
{
    Configure(3);
    dnsmgr->ApplySettings(nullptr);

    // Delete the middle (2.2.2.2) settings
    RemoveConfig(1);
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "3.3.3.3, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con3.example.org, con1.example.org");
}

TEST_F(DNSSettingsManager_MultipleSessions, ThreeEntires_DelMiddle_2)
{
    Configure(3, 2);
    dnsmgr->ApplySettings(nullptr);

    // Delete the middle (2.2.2.2, 2.2.3.3) settings
    RemoveConfig(1);
    dnsmgr->ApplySettings(nullptr);

    ASSERT_STREQ(test_backend->ServerList(), "3.3.3.3, 3.3.4.4, 1.1.1.1, 1.1.2.2");
    ASSERT_STREQ(test_backend->DomainList(), "con3.example.org, con1.example.org");
}


TEST_F(DNSSettingsManager_MultipleSessions, MultipleEvents)
{
    Configure(2, 1, 1);
    dnsmgr->ApplySettings(nullptr);

    // Sanity check
    ASSERT_STREQ(test_backend->ServerList(), "2.2.2.2, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con2.example.org, con1.example.org");

    Configure(1, 1, 3);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "3.3.3.3, 2.2.2.2, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con3.example.org, con2.example.org, con1.example.org");

    RemoveConfig(1);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "3.3.3.3, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con3.example.org, con1.example.org");

    Configure(2, 2, 4);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "5.5.5.5, 5.5.6.6, 4.4.4.4, 4.4.5.5, 3.3.3.3, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con5.example.org, con4.example.org, con3.example.org, con1.example.org");

    RemoveConfig(2);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "5.5.5.5, 5.5.6.6, 4.4.4.4, 4.4.5.5, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con5.example.org, con4.example.org, con1.example.org");

    Configure(1, 1, 6);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "6.6.6.6, 5.5.5.5, 5.5.6.6, 4.4.4.4, 4.4.5.5, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con6.example.org, con5.example.org, con4.example.org, con1.example.org");

    RemoveConfig(3);
    RemoveConfig(4);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "6.6.6.6, 1.1.1.1");
    ASSERT_STREQ(test_backend->DomainList(), "con6.example.org, con1.example.org");

    RemoveConfig(0);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "6.6.6.6");
    ASSERT_STREQ(test_backend->DomainList(), "con6.example.org");

    RemoveConfig(5);
    dnsmgr->ApplySettings(nullptr);
    ASSERT_STREQ(test_backend->ServerList(), "");
    ASSERT_STREQ(test_backend->DomainList(), "");
}
