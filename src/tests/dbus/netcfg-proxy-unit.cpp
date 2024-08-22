//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>//
//  Copyright (C) 2020 - 2023  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file   netcfg-proxy-unit.cpp
 *
 * @brief  Unit test for the NetCfgProxy classes
 *
 */

#include <iostream>
#include <vector>
#include <gdbuspp/connection.hpp>

#include <openvpn/common/base64.hpp>
#include <openvpn/log/logsimple.hpp>

#include "netcfg/proxy-netcfg-mgr.hpp"
#include "netcfg/proxy-netcfg-device.hpp"


int main(int argc, char **argv)
{
    auto conn = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto netcfgmgr = NetCfgProxy::Manager::Create(conn);

    int failures = 0;
    size_t not_our_devs = 0;

    // Create a few devices
    try
    {
        std::cout << "Generating a few devices ... " << std::endl;
        std::vector<std::string> genpaths;
        for (int i = 0; i < 10; i++)
        {
            std::string devname = "testdev" + std::to_string(i);
            std::string devpath = netcfgmgr->CreateVirtualInterface(devname);
            std::cout << "    Device created: " << devname << " ... "
                      << devpath << std::endl;

            genpaths.push_back(devpath);
        }
        std::cout << std::endl;

        std::cout << "Fetching device paths ... " << std::endl;
        DBus::Object::Path::List devpaths = netcfgmgr->FetchInterfaceList();
        DBus::Object::Path::List our_devs{};
        size_t match_count = 0;
        for (const auto &p : devpaths)
        {
            std::cout << "    Device path: " << p << " ... ";
            bool found = false;
            for (const auto &chk : genpaths)
            {
                if (chk == p)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ++not_our_devs;
            }
            else
            {
                ++match_count;
                our_devs.push_back(p);
            }
            std::cout << (found ? "Match!" : "*Unknown (not created by us)*") << std::endl;
        }
        std::cout << std::endl;


        if (genpaths.size() != match_count)
        {
            std::cout << "    Mismatch between number of elements - "
                      << devpaths.size() << " != " << genpaths.size()
                      << std::endl;
            ++failures;
        }
        else
        {
            std::cout << "    Size matches expectations: " << match_count
                      << std::endl
                      << "    Devices not created by us: " << not_our_devs
                      << std::endl
                      << "    Total devices: " << devpaths.size()
                      << std::endl
                      << std::endl;
        }


        std::cout << "Removing devices we created... " << std::endl;
        for (const auto &p : our_devs)
        {
            std::cout << "    Removing: " << p << std::endl;
            NetCfgProxy::Device dev(conn, p);
            dev.Destroy();
        }
        std::cout << std::endl;


        std::cout << "Re-fetching device paths ... " << std::endl;
        devpaths = netcfgmgr->FetchInterfaceList();

        if (devpaths.size() != not_our_devs)
        {
            std::cout << "    FAIL: Devices we created are still available - "
                      << devpaths.size() << std::endl;

            for (const auto &p : devpaths)
            {
                std::cout << "    Still available: " << p << std::endl;
            }
            std::cout << std::endl;
            ++failures;
        }
        else
        {
            std::cout << "    SUCCESS: All our devices removed: "
                      << devpaths.size() - not_our_devs
                      << std::endl;
        }
        std::cout << std::endl;
    }
    catch (std::exception &excp)
    {
        std::cerr << "** FAILURE: " << excp.what() << std::endl;
        return 2;
    }
    std::cout << std::endl
              << "OVERALL RESULT: " << (failures == 0 ? "PASS" : "FAIL")
              << std::endl
              << std::endl;
    return (failures == 0 ? 0 : 3);
}
