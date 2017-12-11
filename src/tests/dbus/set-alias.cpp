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
 * @file   set-alias.cpp
 *
 * @brief  Simple test case setting an alias name to an already imported
 *         configuration profile in the configuration manager.
 */

#include <iostream>
#include "dbus/core.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
        std::string new_alias;
        if (argc < 2 || argc > 3) {
                std::cout << "Usage: " << argv[0]
                          << " <config path> [<alias name>]"
                          << std::endl;
                return 1;
        }

        if( NULL == argv[1]) {
                std::cerr << "** ERROR **  No config object path provided." << std::endl;
                return 2;
        }

        if( NULL != argv[2]) {
                new_alias = std::string(argv[2]);
        }

        try {
                DBusProxy proxy(G_BUS_TYPE_SYSTEM,
                                OpenVPN3DBus_name_configuration,
                                OpenVPN3DBus_interf_configuration,
                                argv[1]);


                try {
                        auto alias = proxy.GetStringProperty("alias");
                        std::cout << "Current alias: " << alias << std::endl;
                        if (!new_alias.empty()) {
                                proxy.SetProperty("alias", new_alias);
                                std::cout << "New alias: " << new_alias << std::endl;
                        }
                } catch (DBusException& err) {
                        std::cerr << "** ERROR ** Failed reading/setting alias property" << std::endl;
                        std::cerr << "-- DEBUG -- " << err.what() << std::endl;
                        return 3;
                }

        } catch (DBusException& err) {
                std::cerr << "** ERROR ** Failed to find the configuration object.  "
                          << "Is the path correct?" << std::endl;
                std::cerr << "-- DEBUG -- " << err.what() << std::endl;
                return 3;
        }
        return 0;
}

