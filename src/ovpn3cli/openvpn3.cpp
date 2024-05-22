//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#define OVPN3CLI_OPENVPN3
#define OVPN3CLI_PROGNAME "OpenVPN 3"
#define OVPN3CLI_PROGDESCR "Command line interface to manage OpenVPN 3 " \
                           "configurations and sessions"

#include "commands/commands.hpp"

//
//  openvpn3 command line utility
//
inline const std::vector<PrepareCommand> registered_commands = {
    prepare_command_version,

    prepare_command_config_import,
    prepare_command_config_manage,
    prepare_command_config_acl,
    prepare_command_config_dump,
    prepare_command_config_remove,
    prepare_command_configs_list,

    prepare_command_session_start,
    // prepare_command_session_manage,
    prepare_command_session_auth,
    prepare_command_session_acl,
    prepare_command_session_stats,
    prepare_command_sessions_list,

    // prepare_command_log,
};

#include "ovpn3cli.hpp" // main() is implemented here
