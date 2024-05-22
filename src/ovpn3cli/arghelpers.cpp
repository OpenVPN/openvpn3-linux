//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   arghelpers.cpp
 *
 * @brief  Argument helper functions, used by the shell-completion feature
 *
 */

#include <string>
#include <sstream>
#include <gdbuspp/connection.hpp>

#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "common/cmdargparser.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "arghelpers.hpp"

/**
 * Retrieves a list of available configuration paths
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_config_paths()
{
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    OpenVPN3ConfigurationProxy confmgr(dbuscon,
                                       Constants::GenPath("configuration"));

    std::stringstream res;
    for (auto &cfg : confmgr.FetchAvailableConfigs())
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
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    OpenVPN3ConfigurationProxy confmgr(dbuscon,
                                       Constants::GenPath("configuration"));

    std::vector<std::string> cfgnames;
    for (const auto &cfgp : confmgr.FetchAvailableConfigs())
    {
        OpenVPN3ConfigurationProxy cfg(dbuscon, cfgp);
        std::string cfgname = cfg.GetName();

        // Filter out duplicates
        bool found = false;
        for (const auto &chk : cfgnames)
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
    for (const auto &n : cfgnames)
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
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

    std::stringstream res;
    for (auto &session : sessmgr->FetchAvailableSessionPaths())
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
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

    std::stringstream res;
    for (const auto &dev : sessmgr->FetchManagedInterfaces())
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
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

    std::vector<std::string> cfgnames;
    for (const auto &session_path : sessmgr->FetchAvailableSessionPaths())
    {
        auto session = sessmgr->Retrieve(session_path);
        std::string cfgname = session->GetConfigName();

        // Filter out duplicates
        bool found = false;
        for (const auto &chk : cfgnames)
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
    for (const auto &n : cfgnames)
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
    for (const ValidOverride &vo : configProfileOverrides)
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
 * @param dbusconn     (optional) DBus::Connection to reuse an existing
 *                     connection
 *
 * @return  Returns a DBus::Object::Path containing the D-Bus configuration
 *          path if a match was found.
 *
 * @throws  Throws CommandException if no or more than one configuration
 *          paths were found.
 */
DBus::Object::Path retrieve_config_path(const std::string &cmd,
                                        const std::string &config_name,
                                        DBus::Connection::Ptr dbusconn)
{
    auto conn = (dbusconn
                     ? dbusconn
                     : DBus::Connection::Create(DBus::BusType::SYSTEM));
    OpenVPN3ConfigurationProxy cfgmgr(conn,
                                      Constants::GenPath("configuration"));

    auto paths = cfgmgr.LookupConfigName(config_name);
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
