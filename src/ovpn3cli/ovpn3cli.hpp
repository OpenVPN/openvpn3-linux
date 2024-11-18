//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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

#include <future>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/mainloop.hpp>
#include "common/cmdargparser.hpp"


int main(int argc, char **argv)
{
    Commands cmds(OVPN3CLI_PROGNAME,
                  OVPN3CLI_PROGDESCR);

    // Register commands
    for (const auto &cmd : registered_commands)
    {
        cmds.RegisterCommand(cmd());
    }

    auto mainloop = DBus::MainLoop::Create();
    auto async_ml = std::async(std::launch::async, [&mainloop]()
                               {
                                   mainloop->Run();
                               });
    usleep(25000); // let the async_ml settle
    int exit_code = 0;

    // Parse the command line arguments and execute the commands given
    try
    {
        // The D-Bus Proxy code requires a running main loop to work
        exit_code = cmds.ProcessCommandLine(argc, argv);
    }
    catch (const DBus::Exception &e)
    {
        std::cerr << simple_basename(argv[0])
                  << "** ERROR **  " << e.GetRawError() << std::endl;

        exit_code = 7;
    }
    catch (CommandException &e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << simple_basename(argv[0]) << "/" << e.getCommand()
                      << ": ** ERROR ** " << e.what() << std::endl;
        }
        exit_code = 8;
    }
    catch (std::exception &e)
    {
        std::cerr << simple_basename(argv[0])
                  << "** ERROR ** " << e.what() << std::endl;
        exit_code = 9;
    }

    if (mainloop->Running())
    {
        mainloop->Stop();
        async_ml.get();
    }
    return exit_code;
}
