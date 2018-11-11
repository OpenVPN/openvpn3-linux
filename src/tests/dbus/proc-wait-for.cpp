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

/**
 * @file   proc-wait-for.cpp
 *
 * @brief  Simple test program listening for specific events from
 *         D-Bus services sending ProcessChange signals.  This will typically
 *         block until a specific PROC_{STARTED,STOPPED,KILLED} event happens.
 *         The program input must be a D-Bus interface name and process name as
 *         well as a numeric reference to the PROC_* event to listen for.
 */

#include <iostream>
#include <string.h>

#include <glib-unix.h>

#include "dbus/core.hpp"
#include "common/utils.hpp"

using namespace openvpn;

struct app_data
{
    const char *interface;
    const char *processname;
    const StatusMinor status;
    GMainLoop *mainloop;
    bool main_running;
    DBus *dbus;
};

void * wait_for_process_loop(gpointer data)
{
    struct app_data *appdata = (struct app_data *) data;

    ProcessSignalWatcher procsub(appdata->dbus->GetConnection());
    StatusMinor res = StatusMinor::UNSET;
    do {
        std::cout << "Waiting for the process signal to happen (timeout 10 seconds)" << std::endl;
        res = procsub.WaitForProcess(std::string(appdata->interface), std::string(appdata->processname));
        std::cout << "status(" << std::to_string((uint8_t) appdata->status) << ") == "
                  << "res(" << std::to_string((uint8_t) res) << ") is "
                  << (res == appdata->status ? "True" : "False") << std::endl;
        std::cout << "Caught : " << StatusMinor_str[(uint8_t) res] << std::endl;
    } while (((uint8_t) res != (uint8_t) appdata->status)
             && appdata->main_running != false );

    g_main_loop_quit(appdata->mainloop);
    g_thread_exit(0);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0] << " <interface name> <process name> <status>" << std::endl;
        std::cout << std::endl;
        std::cout << "Valid status values: " << std::endl;
        std::cout << "   PROC_STARTED -> use " << std::to_string((uint8_t) StatusMinor::PROC_STARTED) << std::endl;
        std::cout << "   PROC_STOPPED -> use " << std::to_string((uint8_t) StatusMinor::PROC_STOPPED) << std::endl;
        std::cout << "   PROC_KILLED -> use " << std::to_string((uint8_t) StatusMinor::PROC_KILLED) << std::endl;
        std::cout << std::endl;
        return 1;
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    std::cout << "Connected to D-Bus" << std::endl;

    struct app_data appdata = {
        .interface = argv[1],
        .processname = argv[2],
        .status  = (StatusMinor) atoi(argv[3]),
        .mainloop = g_main_loop_new(NULL, FALSE),
        .main_running = true,
        .dbus = &dbus
    };

    GThread *workerthread = g_thread_new("worker", wait_for_process_loop, &appdata);

    ProcessSignalProducer procprod(appdata.dbus->GetConnection(), "net.openvpn.v3.test.progsigs", "proc-wait-for");

    g_unix_signal_add(SIGINT, stop_handler, appdata.mainloop);
    g_unix_signal_add(SIGTERM, stop_handler, appdata.mainloop);

    procprod.ProcessChange(StatusMinor::PROC_STARTED);
    g_main_loop_run(appdata.mainloop);
    appdata.main_running = false;
    procprod.ProcessChange(StatusMinor::PROC_STOPPED);
    usleep(500);

    g_thread_join(workerthread);
    g_thread_unref(workerthread);

   return 0;
}
