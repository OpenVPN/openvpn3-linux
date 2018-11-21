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
 * @file   netcfg-dns-direct-file-selftest.cpp
 *
 * @brief  Simple self-test of the NetCfg::DNS::ResolvConfFile() class
 */

#include <iostream>

#define ENABLE_DEBUG
#include "netcfg/dns-direct-file.hpp"

using namespace NetCfg::DNS;

int main()
{
    ResolvConfFile sysresolvconf("/etc/resolv.conf");
    sysresolvconf.Fetch();
    std::cout << sysresolvconf.Dump();
    std::cout << "Writing copy to test-system.conf" << std::endl << std::endl;
    sysresolvconf.SetFilename("test-system.conf");
    sysresolvconf.Apply();

    ResolvConfFile copy(sysresolvconf);
    copy.AddDNSSearch("example.org");
    copy.AddDNSSearch("example.com");
    copy.AddDNSServer("8.8.8.8");
    std::cout << "DUMP OF copy:" << std::endl << copy.Dump() << std::endl;

    ResolvConfFile copy2(copy);
    copy2.ClearDNSSearch();
    copy2.ClearDNSServers();
    copy2.AddDNSSearch("example.net");
    copy2.AddDNSServer("1.1.1.1");
    copy2.AddDNSServer("8.8.4.4");

    std::cout << "DUMP OF copy 2" << std::endl << copy2.Dump();
    std::cout << "Writing copy to test-copy2.conf" << std::endl << std::endl;
    copy2.SetFilename("test-copy2.conf");
    copy2.Apply();

    std::cout << "Comparing:: copy == copy2 ==> "
              << (copy == copy2 ? "Identical - FAIL"
                                     : "Different - PASS")
              << std::endl
              << "Comparing:: copy != copy2 ==> "
              << (copy != copy2 ? "Not identical - PASS"
                                     : "Not different - FAIL")
              << std::endl << std::endl;

    ResolvConfFile fresh("test-fresh.conf");
    fresh.AddDNSSearch("example.org");
    fresh.AddDNSSearch("example.net");
    fresh.AddDNSServer("2606:4700:4700::1111");
    fresh.AddDNSServer("1.0.0.1");
    std::cout << "FRESH" << std::endl << fresh.Dump();
    std::cout << "Writing fresh to test-fresh.conf" << std::endl << std::endl;
    fresh.Apply();

    std::cout << "Overwriting test-fresh.conf from sysresolvconf"
              << " (backup file: backup-test-fresh.conf.bak) " << std::endl;
    sysresolvconf.SetBackupFilename("backup-test-fresh.conf.bak");
    sysresolvconf.SetFilename("test-fresh.conf");
    sysresolvconf.Apply();

    ResolvConfFile backupchk("backup-test-fresh.conf.bak");
    backupchk.Fetch();
    std::cout << "DUMP OF backup-test-fresh.conf.bak" << std::endl
              << backupchk.Dump() << std::endl;

    std::cout << "Comparing:: backupchk == fresh ==> "
              << (backupchk == fresh ? "Identical - PASS"
                                     : "Different - FAIL")
              << std::endl
              << "Comparing:: backupchk != fresh ==> "
              << (backupchk != fresh ? "Not identical - FAIL"
                                     : "Not different - PASS")
              << std::endl << std::endl;

    ResolvConfFile overwritechk("test-fresh.conf");
    overwritechk.Fetch();
    std::cout << "DUMP OF overwritten test-fresh.conf "
              << "(contains sysresolvconf)" << std::endl
              << overwritechk.Dump() << std::endl;

    std::cout << "Comparing:: overwritechk == sysresolvconf ==> "
              << (overwritechk == sysresolvconf ? "Identical - PASS"
                                                : "Different - FAIL")
              << std::endl
              << "Comparing:: overwritechk != sysresolvconf ==> "
              << (overwritechk != sysresolvconf ? "Not identical - FAIL"
                                     : "Not different - PASS")
              << std::endl << std::endl;

    std::cout << "Restoring backup-test-fresh.conf.bak to test-fresh.conf"
              << std::endl;
    sysresolvconf.Restore();

    ResolvConfFile restorechk("test-fresh.conf");
    restorechk.Fetch();
    std::cout << "DUMP OF RESTORE of test-fresh.conf" << std::endl
              << restorechk.Dump() << std::endl;

    std::cout << "DUMP OF sysresolvconf "
              << "(should be unmodified of /etc/resolv.conf)" << std::endl
              << sysresolvconf.Dump() << std::endl;

    return 0;
}

