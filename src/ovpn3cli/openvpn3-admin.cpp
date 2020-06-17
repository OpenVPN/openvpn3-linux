//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
#include "ovpn3cli.hpp"  // main() is implemented here
