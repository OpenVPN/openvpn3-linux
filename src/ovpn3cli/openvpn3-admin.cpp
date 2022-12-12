//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   openvpn3-admin.cpp
 *
 * @brief  Command line utility to manage OpenVPN 3 D-Bus services
 */

#ifndef OVPN3CLI_OPENVPN3ADMIN
#define OVPN3CLI_OPENVPN3ADMIN
#endif
#define OVPN3CLI_PROGNAME "OpenVPN 3 Admin"
#define OVPN3CLI_PROGDESCR "Command line interface to manage OpenVPN 3 " \
                           "D-Bus based services"
#define OVPN3CLI_COMMANDS_LIST command_list_openvpn3admin

#include <openvpn/common/base64.hpp>

#include "commands/commands.hpp"
#include "ovpn3cli.hpp" // main() is implemented here
