//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//

#define OVPN3CLI_OPENVPN3
#define OVPN3CLI_PROGNAME "OpenVPN 3"
#define OVPN3CLI_PROGDESCR "Command line interface to manage OpenVPN 3 " \
                           "configurations and sessions"
#define OVPN3CLI_COMMANDS_LIST command_list_openvpn3

#include "commands/commands.hpp"
#include "ovpn3cli.hpp" // main() is implemented here
