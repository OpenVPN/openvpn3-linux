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
 * @file   version.cpp
 *
 * @brief  Simple generic command for displaying the program version
 */

#include <iostream>
#include "common/cmdargparser.hpp"

/**
 *  Dump various version information, both about the openvpn3 command
 *  but also about the OpenVPN 3 Core library.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 *
 * @return Returns the exit code which will be returned to the calling shell
 */
int cmd_version(ParsedArgs args)
{
    std::cout << get_version("/" + args.GetArgv0()) << std::endl;
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'version' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_version()
{
    SingleCommand::Ptr version;
    version.reset(new SingleCommand("version",
                                    "Show program version information",
                                    cmd_version));

    return version;
}
