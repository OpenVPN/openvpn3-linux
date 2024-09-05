//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   get-config-overrides.cpp
 *
 * @brief  Retrieve and list the configuration profile overrides using
 *         the D-Bus config manager proxy
 */

#include <iostream>
#include <vector>
#include <gdbuspp/connection.hpp>

#include "configmgr/proxy-configmgr.hpp"


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
        return 1;
    }

    auto conn = DBus::Connection::Create(DBus::BusType::SYSTEM);
    OpenVPN3ConfigurationProxy config(conn, argv[1]);

    std::vector<Override> overrides = config.GetOverrides();
    for (const auto &ov : overrides)
    {
        std::string value;
        std::string type;

        if (std::holds_alternative<bool>(ov.value))
        {
            value = (std::get<bool>(ov.value) ? "True" : "False");
            type = "boolean";
        }
        else
        {
            value = std::get<std::string>(ov.value);
            type = "string";
        }

        std::cout << ov.key << " "
                  << "[type: " << type << "]: "
                  << value
                  << std::endl;
    }
    return 0;
}
