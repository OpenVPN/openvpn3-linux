//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#include "config.h"
#ifdef HAVE_CONFIG_VERSION_H
#include "config-version.h"
#endif

#include <iostream>
#include <sstream>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <getopt.h>

#include <client/ovpncli.cpp>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"

#include "commands/config.hpp"
#include "commands/session.hpp"
#include "commands/log.hpp"


/**
 *  Dump various version information, both about the openvpn3 command
 *  but also about the OpenVPN 3 Core library.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 */
int cmd_version(ParsedArgs args)
{
    std::cout << get_version("/openvpn3") << std::endl;
    return 0;
}


int main(int argc, char **argv)
{
    Commands openvpn3("OpenVPN3",
                      "Command line interface to manage OpenVPN 3 "
                      "configurations and sessions");

    //  Add a separate 'version' command, which just prints version information.
    openvpn3.AddCommand("version", "Show OpenVPN 3 version information",
                        cmd_version);

    //  Register commands found in ./commands/*.hpp
    RegisterCommands_config(openvpn3);
    RegisterCommands_session(openvpn3);
    RegisterCommands_log(openvpn3);

    try
    {
        return openvpn3.ProcessCommandLine(argc, argv);
    }
    catch (CommandException& e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
    }
}
