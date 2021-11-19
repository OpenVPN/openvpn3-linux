//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019 - 2021  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2021  David Sommerseth <davids@openvpn.net>
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
 * @file   commands.hpp
 *
 * @brief  Declares the list of available commands for the
 *         command line utilities
 */

#pragma once

#include "common/cmdargparser.hpp"

typedef SingleCommand::Ptr (*PrepareCommand)();

// Command provided in version.cpp
SingleCommand::Ptr prepare_command_version();

// Commands provided in config.cpp
SingleCommand::Ptr prepare_command_config_import();
SingleCommand::Ptr prepare_command_config_manage();
SingleCommand::Ptr prepare_command_config_acl();
SingleCommand::Ptr prepare_command_config_dump();
SingleCommand::Ptr prepare_command_config_remove();
SingleCommand::Ptr prepare_command_configs_list();

// Commands provided in log.cpp
SingleCommand::Ptr prepare_command_log();

// Commands provided in log-service.cpp
SingleCommand::Ptr prepare_command_log_service();

// Commands provided in session.cpp
SingleCommand::Ptr prepare_command_session_start();
SingleCommand::Ptr prepare_command_session_manage();
SingleCommand::Ptr prepare_command_session_auth();
SingleCommand::Ptr prepare_command_session_acl();
SingleCommand::Ptr prepare_command_session_stats();
SingleCommand::Ptr prepare_command_sessions_list();

// Commands provided in netcfg-service.cpp
SingleCommand::Ptr prepare_command_netcfg_service();

// Commands provided in sessionmgr-service.cpp
SingleCommand::Ptr prepare_command_sessionmgr_service();

// Commands provided in variables.cpp
SingleCommand::Ptr prepare_command_variables();


// Gather the complete list of commands.  The order
// here is reflected in the help screen when the main
// program is run.
//
// These lists are fenced with separate macros per
// command line utility, to avoid needing to modify
// multiple files when adding the prepare_command_*()
// functions.

#ifdef OVPN3CLI_OPENVPN3
//
//  openvpn3 command line utility
//
std::vector<PrepareCommand> command_list_openvpn3 = {
    prepare_command_version,

    prepare_command_config_import,
    prepare_command_config_manage,
    prepare_command_config_acl,
    prepare_command_config_dump,
    prepare_command_config_remove,
    prepare_command_configs_list,

    prepare_command_session_start,
    prepare_command_session_manage,
    prepare_command_session_auth,
    prepare_command_session_acl,
    prepare_command_session_stats,
    prepare_command_sessions_list,

    prepare_command_log,
};
#endif // OVPN3CLI_OPENVPN3

#ifdef OVPN3CLI_OPENVPN3ADMIN
//
//  openvpn3-admin command line utility
//
std::vector<PrepareCommand> command_list_openvpn3admin = {
    prepare_command_version,
    prepare_command_variables,

    prepare_command_log_service,
    prepare_command_netcfg_service,
#ifdef HAVE_TINYXML
    prepare_command_sessionmgr_service
#endif
};
#endif // OVPN3CLI_OPENVPN3ADMIN
