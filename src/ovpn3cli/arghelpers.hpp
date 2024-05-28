//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   arghelpers.hpp
 *
 * @brief  Argument helper functions, used by the shell-completion feature
 *
 */

#pragma once

#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>

#include <string>


/**
 * Retrieves a list of available configuration paths
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_config_paths();


/**
 *  Retrieve a list of configuration profiles (by their name) which
 *  is available to the currently calling user
 *
 * @param dbuscon        DBus::Connection object to use for the query
 * @return std::string with configuration profile names, space separated
 */
std::string arghelper_config_names_dbus(DBus::Connection::Ptr dbuscon);


/**
 *  Retrieve a list of configuration profiles (by their name) which
 *  is available to the currently calling user.
 *
 *  This function will create the D-Bus connection internally before
 *  calling arghelper_config_names_dbus()
 *
 * @return std::string with configuration profile names, space separated
 */
std::string arghelper_config_names();


/**
 * Retrieves a list of available configuration paths
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_session_paths();

/**
 * Retrieves a list of managed virtual interfaces
 *
 * @return std::string with all available paths, each separated by space
 */
std::string arghelper_managed_interfaces();


/**
 *  Retrieve a list of configuration profiles (by their name) which
 *  is available to the currently calling user
 *
 * @param dbuscon        DBus::Connection object to use for the query
 * @return std::string with configuration profile names, space separated
 */
std::string arghelper_config_names_sessions_dbus(DBus::Connection::Ptr dbuscon);


/**
 *  Retrieve a list of configuration profiles (by their name) which
 *  is available to the currently calling user
 *
 *  This function will create the D-Bus connection internally before
 *  calling arghelper_config_names_sessions_dbus()
 *
 * @return std::string with configuration profile names, space separated
 */
std::string arghelper_config_names_sessions();


/**
 * List all valid overrides
 * Note we could also list only set overrides but that would require this option to know if
 * config-path is somewhere else on the command line and get its argument
 */
std::string arghelper_unset_overrides();


/**
 *  Provides true/false suggestions for command line completion
 */
std::string arghelper_boolean();


/**
 *  Returns a list of valid log level values
 *
 * @return  std::string with all valid log levels
 */
std::string arghelper_log_levels();

DBus::Object::Path retrieve_config_path(const std::string &cmd,
                                        const std::string &config_name,
                                        DBus::Connection::Ptr dbusconn = nullptr);
