//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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
int cmd_version(ParsedArgs::Ptr args)
{
    std::cout << get_version("/" + args->GetArgv0()) << std::endl;
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
