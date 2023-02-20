//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   fetch-avail-config-paths.cpp
 *
 * @brief  Prints all available configuration paths imported to the
 *         configuration manager (openvpn3-service-configmgr)
 */

#include <iostream>

#include "dbus/core.hpp"
#include "configmgr/proxy-configmgr.hpp"



int main(int argc, char **argv)
{
    OpenVPN3ConfigurationProxy cfgproxy(G_BUS_TYPE_SYSTEM,
                                        OpenVPN3DBus_rootp_configuration);
    for (auto &cfg : cfgproxy.FetchAvailableConfigs())
    {
        std::cout << cfg << std::endl;
    }
    return 0;
}
