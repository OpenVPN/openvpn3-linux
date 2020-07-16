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
 * @file   ovpn3cli.hpp
 *
 * @brief  Generic main() function for OpenVPN 3 command line
 *         programs using the Commands implementation from
 *         commands/cmdargparser.hpp
 *
 *         This file is to be included from a .cpp file and
 *         requires the OVPN3CLI_PROGNAME, OVPN3CLI_PROGDESCR and
 *         OVPN3CLI_COMMANDS_LIST to be defined in advance.
 */

#include "dbus/proxy.hpp"
#include "common/cmdargparser.hpp"


int main(int argc, char **argv)
{
    Commands cmds(OVPN3CLI_PROGNAME,
                  OVPN3CLI_PROGDESCR);

    // Register commands
    for (const auto& cmd : OVPN3CLI_COMMANDS_LIST)
    {
        cmds.RegisterCommand(cmd());
    }

    // Parse the command line arguments and execute the commands given
    try
    {
        return cmds.ProcessCommandLine(argc, argv);
    }
    catch (const DBusProxyAccessDeniedException& e)
    {
        std::cerr << "** ERROR **  " <<  e.what() << std::endl;

        if (std::getenv("OPENVPN_DEBUG") != nullptr)
        {
            std::cerr << "DEBUG: " << std::endl << e.getDebug() << std::endl;
        }
        return 7;
    }
    catch (CommandException& e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
        return 8;
    }
    catch (std::exception& e)
    {
        std::cerr << "** ERROR ** " << e.what() << std::endl;
        return 9;
    }
    std::cerr << "*** EEEK *** This should not have happened" << std::endl;
    return 99;
}
