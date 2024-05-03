//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   getlaststatus.cpp
 *
 * @brief  Simple unit test, testing OpenVPN3SessionProxy::GetLastStatus()
 */

#include <iostream>
#include <gdbuspp/connection.hpp>

#include "sessionmgr/proxy-sessionmgr.hpp"


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <session object path>" << std::endl;
        return 1;
    }

    try
    {
        auto conn = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto sessmgr = SessionManager::Proxy::Manager::Create(conn);
        auto session = sessmgr->Retrieve(argv[1]);
        Events::Status status = session->GetLastStatus();

        std::cout << "        Status: " << status << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "  Status major: [" << status.major
                  << "] " << std::endl;
        std::cout << "  Status minor: [" << status.minor
                  << "] " << std::endl;
        std::cout << "Status message: "
                  << "[len: " << status.message.size()
                  << "] " << status.message
                  << std::endl;
        return 0;
    }
    catch (std::exception &err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}
