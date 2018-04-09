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


#define SHUTDOWN_NOTIF_PROCESS_NAME "openvpn3-service-configmgr"
#include "dbus/core.hpp"
#include "dbus/path.hpp"
#include "configmgr.hpp"
#include "log/dbus-log.hpp"
#include "common/cmdargparser.hpp"
#include "common/utils.hpp"

using namespace openvpn;

static int config_manager(ParsedArgs args)
{
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    // Enable automatic shutdown if the config manager is
    // idling for 1 minute or more.  By idling, it means
    // no configuration files is stored in memory.
    unsigned int idle_wait_min = 3;
    if (args.Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args.GetValue("idle-exit", 0).c_str());
    }


    ConfigManagerDBus cfgmgr(G_BUS_TYPE_SYSTEM);

    unsigned int log_level = 3;
    if (args.Present("log-level"))
    {
        log_level = std::atoi(args.GetValue("log-level", 0).c_str());
    }
    cfgmgr.SetLogLevel(log_level);

    IdleCheck::Ptr idle_exit;
    if (idle_wait_min > 0)
    {
        idle_exit.reset(new IdleCheck(main_loop,
                                      std::chrono::minutes(idle_wait_min)));
        idle_exit->SetPollTime(std::chrono::seconds(30));
        cfgmgr.EnableIdleCheck(idle_exit);
    }
    cfgmgr.Setup();

    if (idle_wait_min > 0)
    {
        idle_exit->Enable();
    }
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    if (idle_wait_min > 0)
    {
        idle_exit->Disable();
        idle_exit->Join();
    }

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Configuration Manager",
                            config_manager);
    argparser.AddOption("log-level", "LOG-LEVEL", true,
                        "Log verbosity level (valid values 0-6, default 3)");
    argparser.AddOption("idle-exit", "MINUTES", true,
                        "How long to wait before exiting if being idle. "
                        "0 disables it (Default: 3 minutes)");


    try
    {
        std::cout << get_version(argv[0]) << std::endl;

        // This program does not require root privileges,
        // so if used - drop those privileges
        drop_root();

        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
