//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   manager-lookupconfigname.cpp
 *
 * @brief  Simple unit test, calling the
 *         LookupConfigName D-Bus method in either
 *         net.openvpn3.v3.configuration or net.openvpn.v3.sessions services
 *         through their respective OpenVPN3*Proxy objects
 */

#include <iostream>
#include <gdbuspp/connection.hpp>

#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
// #include "sessionmgr/proxy-sessionmgr.hpp"



class ManagerProxy
{
  public:
    ManagerProxy(std::string manager)
        : manager(manager)
    {
        conn = DBus::Connection::Create(DBus::BusType::SYSTEM);
        if ("configmgr" == manager)
        {
            cfgmgr.reset(new OpenVPN3ConfigurationProxy(conn,
                                                        Constants::GenPath("configuration")));
        }
        else if ("sessionmgr-NOT-MIGRATED-YET" == manager)
        {
            // sessmgr.reset(new OpenVPN3SessionMgrProxy(G_BUS_TYPE_SYSTEM));
        }
        else
        {
            throw DBus::Exception("manager-lookupconfigname",
                                  "Invalid manager name:" + manager);
        }
    }

    ~ManagerProxy() = default;

    std::vector<std::string> LookupConfigName(const std::string cfgname)
    {
        if ("configmgr" == manager)
        {
            return cfgmgr->LookupConfigName(cfgname);
        }
        else if ("sessionmgr" == manager)
        {
            //return sessmgr->LookupConfigName(cfgname);
        }
        throw DBus::Exception("manager-lookupconfigname",
                              "Invalid manager name:" + manager);
    }

  private:
    DBus::Connection::Ptr conn = nullptr;
    std::string manager = "";
    std::unique_ptr<OpenVPN3ConfigurationProxy> cfgmgr = nullptr;
    // std::unique_ptr<OpenVPN3SessionMgrProxy> sessmgr = nullptr;
};



int main(int argc, char **argv)
{
    if (3 > argc)
    {
        std::cerr << "Usage: " << argv[0] << " <configmgr|sessionmgr> "
                  << "<config name>" << std::endl;
        return 1;
    }

    ManagerProxy mgrprx(argv[1]);
    std::cout << "Lookup up configuration paths for '"
              << argv[2] << "' in the " << argv[1] << std::endl;
    unsigned int i = 0;
    for (const auto &p : mgrprx.LookupConfigName(argv[2]))
    {
        std::cout << " - " << p << std::endl;
        ++i;
    }
    std::cout << "Found " << std::to_string(i) << " object"
              << (i != 1 ? "s" : "") << std::endl;
    return 0;
}
