//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
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
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
        return 1;
    }

    DBus dbusobj(G_BUS_TYPE_SYSTEM);
    dbusobj.Connect();

    OpenVPN3ConfigurationProxy config(dbusobj, argv[1]);

    std::vector<OverrideValue> overrides = config.GetOverrides();
    for (const auto &ov : overrides)
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
