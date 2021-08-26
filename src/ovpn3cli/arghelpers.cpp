//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
 * @file   arghelpers.cpp
 *
 * @brief  Argument helper functions, used by the shell-completion feature
 *
 */

#include <string>
#include <sstream>

#include "configmgr/proxy-configmgr.hpp"
#include "common/cmdargparser.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"

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
 * Retrieves a list of all available configuration profile names
 *
 * @return std::string with all available profile names, each separated
 *         by space
 */
std::string arghelper_config_names()
{
    DBus conn(G_BUS_TYPE_SYSTEM);
    conn.Connect();
    OpenVPN3ConfigurationProxy confmgr(conn, OpenVPN3DBus_rootp_configuration);

    std::vector<std::string> cfgnames;
    for (const auto& cfgp : confmgr.FetchAvailableConfigs())
    {
        OpenVPN3ConfigurationProxy cfg(conn, cfgp);
        std::string cfgname = cfg.GetStringProperty("name");

        // Filter out duplicates
        bool found = false;
        for (const auto& chk : cfgnames)
        {
            if (chk == cfgname)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            cfgnames.push_back(cfgname);
        }
    }

    // Generate the final string which will be returned
    std::stringstream res;
    for (const auto& n : cfgnames)
    {
        if (n.empty())
        {
            continue;
        }
        res << n << " ";
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
    OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

    std::stringstream res;
    for (auto& session : sessmgr.FetchAvailableSessionPaths())
    {
        if (session.empty())
        {
            continue;
        }
        res << session << " ";
    }
    return res.str();
}


std::string arghelper_managed_interfaces()
{
    OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

    std::stringstream res;
    for (const auto& dev : sessmgr.FetchManagedInterfaces())
    {
        if (dev.empty())
        {
            continue;
        }
        res << dev << " ";
    }
    return res.str();
}

/**
 * Retrieves a list of all available configuration profile names for
 * currently running sessions.
 *
 * @return std::string with all available profile names, each separated
 *         by space
 */
std::string arghelper_config_names_sessions()
{
    DBus conn(G_BUS_TYPE_SYSTEM);
    conn.Connect();
    OpenVPN3SessionMgrProxy sessmgr(conn);

    std::vector<std::string> cfgnames;
    for (const auto& sesp : sessmgr.FetchAvailableSessionPaths())
    {
        OpenVPN3SessionProxy sess(conn, sesp);
        std::string cfgname = sess.GetStringProperty("config_name");

        // Filter out duplicates
        bool found = false;
        for (const auto& chk : cfgnames)
        {
            if (chk == cfgname)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            cfgnames.push_back(cfgname);
        }
    }

    // Generate the final string which will be returned
    std::stringstream res;
    for (const auto& n : cfgnames)
    {
        if (n.empty())
        {
            continue;
        }
        res << n << " ";
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


/**
 *  Retrieve a configuration path based on a configuration name
 *
 * @param cmd          std::string containing the command name to use in
 *                     case of errors
 * @param config_name  std::string containing the configuration name to
 *                     look up
 *
 * @return  Returns a std::string containing the D-Bus configuration path
 *          if a match was found.
 *
 * @throws  Throws CommandException if no or more than one configuration
 *          paths were found.
 */
std::string retrieve_config_path(const std::string& cmd,
                                 const std::string& config_name)
{
    OpenVPN3ConfigurationProxy cfgmgr(G_BUS_TYPE_SYSTEM,
                                      OpenVPN3DBus_rootp_configuration);
    std::vector<std::string> paths = cfgmgr.LookupConfigName(config_name);
    if (0 == paths.size())
    {
        throw CommandException(cmd,
                               "No configuration profiles found");
    }
    else if (1 < paths.size())
    {
        throw CommandException(cmd,
                               "More than one configuration profile was "
                               "found with the given name");
    }
    return paths.at(0);
}
