//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   commands.hpp
 *
 * @brief  Declares the list of available commands for the
 *         command line utilities
 */

#pragma once

#include <functional>

#include "common/cmdargparser.hpp"

using PrepareCommand = std::function<SingleCommand::Ptr()>;

// Command provided in version.cpp
SingleCommand::Ptr prepare_command_version();

// Command provided in version-services.cpp
SingleCommand::Ptr prepare_command_version_services();

// Commands provided in config-import.cpp
SingleCommand::Ptr prepare_command_config_import();

// Commands provided in config.cpp
SingleCommand::Ptr prepare_command_config_manage();
SingleCommand::Ptr prepare_command_config_acl();
SingleCommand::Ptr prepare_command_config_dump();
SingleCommand::Ptr prepare_command_config_remove();

// Commands provided in config/configs-list.cpp
SingleCommand::Ptr prepare_command_configs_list();

// Commands provided in init-config.cpp
SingleCommand::Ptr prepare_command_initcfg();

// Commands provided in journal.cpp
SingleCommand::Ptr prepare_command_journal();

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
