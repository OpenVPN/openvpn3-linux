//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   get-service-version-prop.cpp
 *
 * @brief  Simple test program to test looking up the 'version'
 *         property in a D-Bus service
 */

#include <iostream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>



int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0]
                  << " <service name> <service d-bus path> <service interface>" << std::endl;
        return 1;
    }

    try
    {
        std::string service(argv[1]);
        std::string path(argv[2]);
        std::string interface(argv[3]);

        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto proxy = DBus::Proxy::Client::Create(dbuscon, service);
        auto prxqry = DBus::Proxy::Utils::Query::Create(proxy);

        std::cout << "Service version: "
                  << prxqry->ServiceVersion(path, interface)
                  << std::endl;
        return 0;
    }
    catch (std::exception &err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
