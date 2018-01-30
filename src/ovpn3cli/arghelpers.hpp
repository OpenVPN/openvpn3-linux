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
 * @file   arghelpers.hpp
 *
 * @brief  Argument helper functions, used by the shell-completion feature
 *
 */

#ifndef OPENVPN3_ARGHELPERS_HPP
#define OPENVPN3_ARGHELPERS_HPP

/**
 * Retrieves a list of available configuration paths
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_config_paths()
{
    OpenVPN3ConfigurationProxy confmgr(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration);

    std::stringstream res;
    for (auto& cfg : confmgr.FetchAvailableConfigs())
    {
        if (cfg.empty())
        {
            continue;
        }
        res << cfg << " ";
    }
    return res.str();
}


/**
 * Retrieves a list of available configuration paths
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_session_paths()
{
    OpenVPN3SessionProxy sessmgr(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_sessions);

    std::stringstream res;
    for (auto& session : sessmgr.FetchAvailableSessions())
    {
        if (session.empty())
        {
            continue;
        }
        res << session << " ";
    }
    return res.str();
}


/**
 *  Provides true/false suggestions for command line completion
 */
std::string arghelper_boolean()
{
    return "false true";
}
#endif // OPENVPN3_ARGHELPERS_HPP
