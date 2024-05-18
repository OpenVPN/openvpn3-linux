//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   openvpn3-admin.cpp
 *
 * @brief  Command line utility to manage OpenVPN 3 D-Bus services
 */

#define OVPN3CLI_OPENVPN3ADMIN
#define OVPN3CLI_PROGNAME "OpenVPN 3 Admin"
#define OVPN3CLI_PROGDESCR "Command line interface to manage OpenVPN 3 " \
                           "D-Bus based services"

#include <openvpn/common/base64.hpp>

#include "commands/commands.hpp"

//
//  openvpn3-admin command line utility
//
inline std::vector<PrepareCommand> registered_commands = {
    prepare_command_version,
    prepare_command_variables,

#ifdef HAVE_SYSTEMD
    // prepare_command_journal,
#endif
    // prepare_command_log_service,
    // prepare_command_netcfg_service,
    // prepare_command_sessionmgr_service,
    // prepare_command_initcfg,
};

#include "ovpn3cli.hpp" // main() is implemented here
