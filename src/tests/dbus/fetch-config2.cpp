//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
 * @file   fetch-config2.cpp
 *
 * @brief  Dumps a specific configuration stored in the configuration manager.
 *         This variant uses the Configuration Manager Proxy object to acheive
 *         this task.  Input to the program is just the D-Bus configuration path
 */

#include <iostream>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
        if (argc != 2) {
            std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
            return 1;
        }

        //        OpenVPN3ConfigurationProxy config(G_BUS_TYPE_SYSTEM, argv[1]);
        DBus dbusobj(G_BUS_TYPE_SYSTEM);
        dbusobj.Connect();

        OpenVPN3ConfigurationProxy config(dbusobj, argv[1]);

        std::cout << "Configuration: " << std::endl;
        std::cout << "  - Name:       " << config.GetStringProperty("name") << std::endl;
        std::cout << "  - Read only:  " << (config.GetBoolProperty("readonly") ? "Yes" : "No") << std::endl;
        std::cout << "  - Persistent: " << (config.GetBoolProperty("persistent") ? "Yes" : "No") << std::endl;
        std::cout << "  - Usage:      " << (config.GetBoolProperty("single_use") ? "Once" : "Multiple times") << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << config.GetJSONConfig() << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "** DONE" << std::endl;

        return 0;
}

