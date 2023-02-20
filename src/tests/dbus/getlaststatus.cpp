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
        std::string session_path(argv[1]);
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, session_path);
        StatusEvent status = session.GetLastStatus();

        std::cout << "        Status: " << status << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "  Status major: [" << std::to_string((unsigned int)status.major)
                  << "] " << std::endl;
        std::cout << "  Status minor: [" << std::to_string((unsigned int)status.minor)
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
