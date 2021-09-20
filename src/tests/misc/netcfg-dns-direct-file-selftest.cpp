//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-dns-direct-file-selftest.cpp
 *
 * @brief  Simple self-test of the NetCfg::DNS::ResolvConfFile() class
 */

#include "config.h"

#include <iostream>
#include <cstdio>
#include <sys/stat.h>

#include "netcfg/dns/settings-manager.hpp"
#include "netcfg/dns/resolvconf-file.hpp"

using namespace NetCfg::DNS;

bool file_exists (const std::string& fname) noexcept
{
    struct stat buffer;
    return (stat (fname.c_str(), &buffer) == 0);
}


int main()
{
    ResolvConfFile::Ptr sysresolvconf = new ResolvConfFile("/etc/resolv.conf");

    sysresolvconf->Debug_Fetch();
    std::cout << "DUMP OF sysresolvconf" << std::endl << sysresolvconf->Dump();
    std::cout << "Writing copy to test-system.conf" << std::endl << std::endl;
    sysresolvconf->SetFilename("test-system.conf");
    sysresolvconf->Debug_Write();

    ResolverSettings::Ptr settings = new ResolverSettings(1);
    settings->AddSearchDomain("example.org");
    settings->AddSearchDomain("example.com");
    settings->AddNameServer("1.1.1.1");
    settings->AddNameServer("8.8.8.8");
    std::cout << "(Before Enable() call) settings: " << settings << std::endl;
    settings->Enable();
    std::cout << "(After Enable() call)  settings: " << settings << std::endl;
    sysresolvconf->Apply(settings);
    std::cout << "DUMP OF modified sysresolvconf:" << std::endl << sysresolvconf->Dump() << std::endl;
    sysresolvconf->SetFilename("modified-system.conf");
    sysresolvconf->Debug_Write();

    ResolvConfFile fresh1("fresh1.conf");
    ResolverSettings::Ptr settings2 = new ResolverSettings(2);
    settings2->ClearSearchDomains();
    settings2->ClearNameServers();
    settings2->AddSearchDomain("example.net");
    settings2->AddNameServer("1.0.0.1");
    settings2->AddNameServer("8.8.4.4");
    settings2->Enable();
    std::cout << "settings2: " << settings2 << std::endl;
    fresh1.Apply(settings2);
    fresh1.Debug_Write();

    std::cout << "DUMP OF fresh1/fresh2" << std::endl << fresh1.Dump();
    std::cout << "Writing fresh to fresh2.conf" << std::endl << std::endl;
    fresh1.SetFilename("fresh2.conf");
    fresh1.Debug_Write();


    std::cout << std::endl << std::endl << "== Backup tests == "<< std::endl;

    // Create a new file and populate it
    ResolvConfFile::Ptr backuptest_new = new ResolvConfFile("backuptest-start.conf");
    backuptest_new->Apply(settings);
    auto backup_new_servers= backuptest_new->Debug_Get_dns_servers();
    auto backup_new_search= backuptest_new->Debug_Get_search_domains();

    std::cout << "DUMP OF backuptest_new" << std::endl << backuptest_new->Dump();
    for (const auto& e : backup_new_servers)
    {
        std::cout << " ~ " << e << std::endl;
    }
    for (const auto& e : backup_new_search)
    {
        std::cout << " ~ " << e << std::endl;
    }

    backuptest_new->Debug_Write();
    backuptest_new->SetFilename("backuptest-new.conf");
    backuptest_new->Apply(settings);
    backuptest_new->Debug_Write();
    backuptest_new.reset();

    std::cout << "backuptest-start.conf file created? "
              << (file_exists("backuptest-start.conf") ? "yes" : "NO!!!")
              << std::endl;


    // Reopen the backup test file and modify it
    ResolvConfFile::Ptr backuptest_start = new ResolvConfFile("backuptest-start.conf", "backuptest-backup.conf");
    backuptest_start->Debug_Fetch();
    std::cout << "DUMP OF backuptest_start [1] " << std::endl << backuptest_start->Dump();
    backuptest_start->Apply(settings2);
    std::cout << "DUMP OF backuptest_start [2] " << std::endl << backuptest_start->Dump();
    backuptest_start->Commit(nullptr);

    std::cout << "Backup file created? "
              << (file_exists("backuptest-backup.conf") ? "yes" : "NO!!!")
              << std::endl;

    ResolvConfFile::Ptr backup_chk = new ResolvConfFile("backuptest-backup.conf");
    backup_chk->Debug_Fetch();

    auto backup_chk_servers= backup_chk->Debug_Get_dns_servers();
    std::cout << "Backup content of DNS servers matches? (original backuptest-start.conf == backuptest-backup.conf) "
              <<  (backup_new_servers == backup_chk_servers ? "yes" : "NO!!!")
              << std::endl;

    for (const auto& e : backup_chk_servers)
    {
        std::cout << " = " << e << std::endl;
    }

    auto backup_chk_search= backup_chk->Debug_Get_search_domains();
    std::cout << "Backup content of domain search matches? (original backuptest-start.conf == backuptest-backup.conf) "
              <<  (backup_new_servers == backup_chk_servers ? "yes" : "NO!!!")
              << std::endl;

    for (const auto& e : backup_chk_search)
    {
        std::cout << " = " << e << std::endl;
    }

    backup_chk.reset();
    backuptest_start.reset();

    std::cout << "Backup file removed? "
              << (file_exists("backuptest-backup.conf") ? "NO!!!" : "yes")
              << std::endl;

    return 0;
}

