//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   fetch-avail-session-paths.cpp
 *
 * @brief  Prints all active sessions paths from the
 *         sessionmanager (openvpn3-service-sessionmgr)
 */

#include <iostream>

#include "dbus/core.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"



int main(int argc, char **argv)
{
    OpenVPN3SessionMgrProxy sessproxy(G_BUS_TYPE_SYSTEM);

    for (auto &path : sessproxy.FetchAvailableSessionPaths())
    {
        std::cout << path << std::endl;
    }
    return 0;
}
