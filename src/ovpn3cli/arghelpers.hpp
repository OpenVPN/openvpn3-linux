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
 * List all valid overrides
 * Note we could also list only set overrides but that would require this option to know if
 * config-path is somewhere else on the command line and get its argument
 */
std::string arghelper_unset_overrides()
{
    std::stringstream out;
    for (const ValidOverride & vo: configProfileOverrides)
    {
        out << vo.key << " ";
    }
    return out.str();
}


/**
 *  Provides true/false suggestions for command line completion
 */
std::string arghelper_boolean()
{
    return "false true";
}


/**
 *  Generates a list of integers, based on the given start and end values
 *
 * @param start   Start number of the range
 * @param end     End number of the range
 *
 * @return std::string with all the numbers on a single line
 */
static std::string intern_arghelper_integer_range(int start, int end)
{
    std::stringstream out;
    for (int i = start; i <= end; i++)
    {
        out << std::to_string(i) << " ";
    }
    return out.str();
}


/**
 *  Returns a list of valid log level values
 *
 * @return  std::string with all valid log levels
 */
std::string arghelper_log_levels()
{
    // See dbus-log.hpp - LogFilter::SetLogLevel() and
    // LogFilter::_LogFilterAllow() for details
    return intern_arghelper_integer_range(0, 6);
}

#endif // OPENVPN3_ARGHELPERS_HPP
