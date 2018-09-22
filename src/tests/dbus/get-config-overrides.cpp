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
 * @file   get-config-overrides.cpp
 *
 * @brief  Retrieve and list the configuration profile overrides using
 *         the D-Bus config manager proxy
 */

#include <iostream>
#include <vector>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
        if (argc != 2) {
            std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
            return 1;
        }

        DBus dbusobj(G_BUS_TYPE_SYSTEM);
        dbusobj.Connect();

        OpenVPN3ConfigurationProxy config(dbusobj, argv[1]);

        std::vector<OverrideValue> overrides = config.GetOverrides();
        for (const auto& ov : overrides)
        {
            std::string value;
            std::string type;

            switch (ov.override.type)
            {
            case OverrideType::boolean:
                    value = (ov.boolValue ? "True" : "False");
                    type = "boolean";
                    break;

            case OverrideType::string:
                    value = ov.strValue;
                    type = "string";
                    break;

            case OverrideType::invalid:
                    value = "(invalid)";
                    type = "(invalid)";
                    break;

            default:
                    value = "(unknown)";
                    type = "(unknown)";
            }
            std::cout << ov.override.key << " "
                      << "[type: " << type << "]: "
                      << value
                      << std::endl;
        }
        return 0;
}

