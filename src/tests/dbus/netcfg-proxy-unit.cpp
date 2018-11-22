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
 * @file   netcfg-proxy-unit.cpp
 *
 * @brief  Unit test for the NetCfgProxy classes
 *
 */

#include <iostream>
#include <vector>

#include "dbus/core.hpp"
#include "netcfg/proxy-netcfg.hpp"


int main(int argc, char **argv)
{
    DBus conn(G_BUS_TYPE_SYSTEM);
    conn.Connect();

    NetCfgProxy::Manager netcfgmgr(conn.GetConnection());
    int failures = 0;

    // Create a few devices
    try
    {
        std::cout << "Generating a few devices ... " << std::endl;
        std::vector<std::string> genpaths;
        for (int i=0; i < 10; i++)
        {
            std::string devname ="testdev" + std::to_string(i);
            std::string devpath = netcfgmgr.CreateVirtualInterface(devname);
            std::cout << "    Device created: " << devname << " ... "
                      << devpath << std::endl;;
            genpaths.push_back(devpath);
        }
        std::cout << std::endl;

        std::cout << "Fetching device paths ... " << std::endl;
        std::vector<std::string> devpaths = netcfgmgr.FetchInterfaceList();

        if (devpaths.size() != genpaths.size())
        {
            std::cout << "    Mismatch between number of elements - "
                      << devpaths.size() << " != " << genpaths.size()
                      << std::endl;
            ++failures;
        }
        else
        {
            std::cout << "    Size mathces expectaions: " << devpaths.size()
                      << std::endl;
        }

        for (const auto& p : devpaths)
        {
            std::cout << "    Device path: " << p << " ... ";
            bool found = false;
            for (const auto& chk : genpaths)
            {
                if (chk == p)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ++failures;
            }
            std::cout << (found ? "Match!" : "*NOT FOUND*") << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Removing devices... " << std::endl;
        for (const auto& p : devpaths)
        {
            std::cout << "    Removing: " << p << std::endl;
            NetCfgProxy::Device dev(conn.GetConnection(), p);
            dev.Destroy();
        }
        std::cout << std::endl;


        std::cout << "Re-fetching device paths ... " << std::endl;
        devpaths = netcfgmgr.FetchInterfaceList();

        if (devpaths.size() != 0)
        {
            std::cout << "    FAIL: Devices still available - "
                      << devpaths.size() << std::endl;
            ++failures;
        }
        else
        {
            std::cout << "    SUCCESS: Empty device list: " << devpaths.size()
                      << std::endl;
        }
        std::cout << std::endl;
    }
    catch (std::exception& excp)
    {
        std::cerr << "** FAILURE: " << excp.what() << std::endl;;
        return 2;
    }
    std::cout << std::endl
              << "OVERALL RESULT: " << (failures == 0 ? "PASS" : "FAIL")
              << std::endl << std::endl;
    return (failures == 0 ? 0 : 3);
}
