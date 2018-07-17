//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN, Inc. <sales@openvpn.net>
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
 * @file   getlastlogevent.cpp
 *
 * @brief  Simple unit test, testing OpenVPN3SessionProxy::GetLastLogEvent()
 */

#include <iostream>

#include "sessionmgr/proxy-sessionmgr.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << "<session object path>" << std::endl;
        return 1;
    }

    try
    {
        std::string session_path(argv[1]);
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, session_path);
        LogEvent log = session.GetLastLogEvent();

        std::cout << "LogEvent: " << log << std::endl;
        std::cout << "     Log Group: ["
                  << std::to_string((unsigned int) log.group)
                  << "] " << std::endl;
        std::cout << "  Log Category: ["
                  << std::to_string((unsigned int) log.category)
                  << "] " << std::endl;
        std::cout << "   Log message: " << "[len: " << log.message.size()
                  << "] " << log.message
                  << std::endl;
        return 0;
    }
    catch (std::exception& err)
    {
        std::cout << "** ERROR ** " << err.what() << std::endl;
        return 2;
    }
}


