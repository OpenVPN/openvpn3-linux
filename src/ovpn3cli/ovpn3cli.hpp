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


/**
 *   Container used to synchronize the execution between
 *   an async thread running the glib2 mainloop and the main thread
 *   running main().
 */
struct mainloop_sync
{
    std::mutex mtx;
    std::condition_variable cv;
    bool ml_running = false;
};


/**
 *  Helper function which is run by the main loop in glib2 as soon as
 *  the main loop has been started.  This is triggered by a g_idle_add()
 *  call before starting the async thread running the main loop.
 *
 *  The purpose is to let the main() function wait for the glib2 main loop
 *  to properly settle and get started.
 *
 *  The data pointer points at a mainloop_sync object which contains
 *  the needed mutex and condvar to for the synchronisation.
 *
 * @param data   pointer to the mainloop_sync object
 * @return int   Will always return G_SOURCE_REMOVE.  This is to avoid
 *               this function being called more than once.
 */
int mainloop_running(void *data)
{
    auto mls = static_cast<mainloop_sync *>(data);

    // Signal the wait_for_mainloop() function so it can be unlocked
    std::lock_guard lg(mls->mtx);
    mls->ml_running = true;
    mls->cv.notify_all();

    // This should only be run once
    return G_SOURCE_REMOVE;
}


/**
 *  Helper function to block further execution until the glib2 main loop
 *  has properly started.  See mainloop_running() for details.
 *
 * @param ml_sync  mainloop_sync object keeping the lock synchronisation
 */
void wait_for_mainloop(mainloop_sync &ml_sync)
{
    // Wait for the mainloop_running() function to be called
    std::unique_lock<std::mutex> mls_lock(ml_sync.mtx);
    ml_sync.cv.wait(mls_lock, [&ml_sync]()
                    {
                        return ml_sync.ml_running;
                    });
}


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

    // Run the mainloop_running() function once when the glib2
    // main loop starts running
    mainloop_sync ml_sync;
    g_idle_add(&mainloop_running, &ml_sync);

    // start the main loop in a separate thread
    auto async_ml = std::async(std::launch::async, [&mainloop]()
                               {
                                   mainloop->Run();
                               });

    // Wait for the async thread to have started the glib2 main loop
    wait_for_mainloop(ml_sync);

    // Parse the command line arguments and execute the commands given
    int exit_code = 0;
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
