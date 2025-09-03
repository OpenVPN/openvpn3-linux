//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   fetch-config2.cpp
 *
 * @brief  Dumps a specific configuration stored in the configuration manager.
 *         This variant uses the Configuration Manager Proxy object to achieve
 *         this task.  Input to the program is just the D-Bus configuration path
 */

#include <iostream>
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

    std::cout << "Configuration: " << std::endl;
    std::cout << "  - Name:       " << config.GetName() << std::endl;
    std::cout << "  - Read only:  " << (config.GetSealed() ? "Yes" : "No") << std::endl;
    std::cout << "  - Persistent: " << (config.GetPersistent() ? "Yes" : "No") << std::endl;
    std::cout << "  - Usage:      " << (config.GetSingleUse() ? "Once" : "Multiple times") << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << config.GetJSONConfig() << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "** DONE" << std::endl;

    return 0;
}
