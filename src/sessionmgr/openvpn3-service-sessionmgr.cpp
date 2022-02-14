//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <glib-unix.h>

#define SHUTDOWN_NOTIF_PROCESS_NAME "openvpn3-service-sessionmgr"
#include "dbus/core.hpp"
#include "sessionmgr.hpp"
#include "log/ansicolours.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "log/proxy-log.hpp"
#include "common/cmdargparser.hpp"

#include <openvpn/common/base64.hpp>

using namespace openvpn;

static int session_manager(ParsedArgs::Ptr args)
{
    std::cout << get_version(args->GetArgv0()) << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

    // Enable automatic shutdown if the config manager is
    // idling for 1 minute or more.  By idling, it means
    // no VPN sessions are registered.
    unsigned int idle_wait_min = 3;
    if (args->Present("idle-exit"))
    {
        idle_wait_min = std::atoi(args->GetValue("idle-exit", 0).c_str());
    }

    // Open a log destination, if requested
    std::ofstream logfs;
    std::ostream  *logfile = nullptr;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    if (args->Present("log-file"))
    {
        std::string fname = args->GetValue("log-file", 0);

        if ("stdout:" != fname)
        {
            logfs.open(fname.c_str(), std::ios_base::app);
            logfile = &logfs;
        }
        else
        {
            logfile = &std::cout;
        }

        if (args->Present("colour"))
        {
            colourengine.reset(new ANSIColours());
             logwr.reset(new ColourStreamWriter(*logfile,
                                                colourengine.get()));
        }
        else
        {
            logwr.reset(new StreamLogWriter(*logfile));
        }
    }

    bool signal_broadcast = args->Present("signal-broadcast");
    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    SessionManagerDBus sessmgr(dbus.GetConnection(), logwr.get(),
                               signal_broadcast);

    LogServiceProxy::Ptr logsrvprx = nullptr;
    if (!signal_broadcast)
    {
        logsrvprx = LogServiceProxy::AttachInterface(dbus.GetConnection(),
                                                     OpenVPN3DBus_interf_sessions);
    }

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }
    sessmgr.SetManagerLogLevel(log_level);

    IdleCheck::Ptr idle_exit;
    if (idle_wait_min > 0)
    {
        idle_exit.reset(new IdleCheck(main_loop,
                                      std::chrono::minutes(idle_wait_min)));
        sessmgr.EnableIdleCheck(idle_exit);
    }
    else
    {
        // If we don't use the IdleChecker, handle these signals
        // in via the stop_handler instead
        g_unix_signal_add(SIGINT, stop_handler, main_loop);
        g_unix_signal_add(SIGTERM, stop_handler, main_loop);
    }
    sessmgr.Setup();

    if (idle_wait_min > 0)
    {
        idle_exit->Enable();
    }
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    if (logsrvprx)
    {
        logsrvprx->Detach(OpenVPN3DBus_interf_sessions);
    }

    if (idle_wait_min > 0)
    {
        idle_exit->Disable();
        idle_exit->Join();
    }

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 Session Manager",
                            session_manager);
    argparser.AddVersionOption();
    argparser.AddOption("log-level", "LOG-LEVEL", true,
                        "Log verbosity level (valid values 0-6, default 3)");
    argparser.AddOption("log-file", "FILE" , true,
                        "Write log data to FILE.  Use 'stdout:' for console logging.");
    argparser.AddOption("colour", 0,
                        "Make the log lines colourful");
    argparser.AddOption("signal-broadcast", 0,
                        "Broadcast all D-Bus signals instead of targeted unicast");
    argparser.AddOption("idle-exit", "MINUTES", true,
                        "How long to wait before exiting if being idle. "
                        "0 disables it (Default: 3 minutes)");

    try
    {
        // This program does not require root privileges,
        // so if used - drop those privileges
        drop_root();

        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException& excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        return 2;
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
