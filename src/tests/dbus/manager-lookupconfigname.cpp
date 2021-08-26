//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
 * @file   manager-lookupconfigname.cpp
 *
 * @brief  Simple unit test, calling the
 *         LookupConfigName D-Bus method in either
 *         net.openvpn3.v3.configuration or net.openvpn.v3.sessions services
 *         through their respective OpenVPN3*Proxy objects
 */

#include <iostream>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"

class ManagerProxy
{
public:
    ManagerProxy(std::string manager)
        : manager(manager)
    {
        if ("configmgr" == manager)
        {
            cfgmgr.reset(new OpenVPN3ConfigurationProxy(G_BUS_TYPE_SYSTEM,
                                                        OpenVPN3DBus_rootp_configuration));
        }
        else if ("sessionmgr" == manager)
        {
            sessmgr.reset(new OpenVPN3SessionMgrProxy(G_BUS_TYPE_SYSTEM));
        }
        else
        {
            THROW_DBUSEXCEPTION("manager-lookupconfigname",
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
            return sessmgr->LookupConfigName(cfgname);
        }
        THROW_DBUSEXCEPTION("manager-lookupconfigname",
                            "Invalid manager name:" + manager);
    }

private:
    std::string manager = "";
    std::unique_ptr<OpenVPN3ConfigurationProxy> cfgmgr = nullptr;
    std::unique_ptr<OpenVPN3SessionMgrProxy> sessmgr = nullptr;
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
              << argv[2] << "' in the "<< argv[1] << std::endl;
    unsigned int i = 0;
    for (const auto& p : mgrprx.LookupConfigName(argv[2]))
    {
        std::cout << " - " << p << std::endl;
        ++i;
    }
    std::cout << "Found " << std::to_string(i) << " object"
              << (i != 1 ? "s" : "") << std::endl;
    return 0;
}


