//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   logfwd-listener.cpp
 *
 * @brief  Attaches to Log signals on a running session.  This requires
 *         the net.openvpn.v3.log service to support ProxyLogEvents
 *
 */

#include <iostream>
#include <string.h>

#include <glib-unix.h>

#include "dbus/core.hpp"
#include "client/statusevent.hpp"
#include "common/utils.hpp"
#include "log/dbus-logfwd.hpp"

using namespace openvpn;


class LogFwdSubscription : public LogForwardBase<LogFwdSubscription>
{
public:
    LogFwdSubscription(GMainLoop* mainloop,
                       DBus& dbc,
                       const std::string& path,
                       const std::string& interf = "")
        : LogForwardBase(dbc, interf, path),
          main_loop(mainloop)
    {
    }

protected:
    void ConsumeLogEvent(const std::string sender_name,
                         const std::string interface_name,
                         const std::string obj_path,
                         const LogEvent &logev) final
    {
        std::cout << " LogEvent{sender=" << sender_name
                << ", interface=" << interface_name
                << ", path=" << obj_path << "} : " << logev << std::endl;
    }

    void StatusChangeEvent(const std::string sender_name,
                           const std::string interface_name,
                           const std::string obj_path,
                           const StatusEvent& status) final
    {
        std::cout << " StatusChange{sender=" << sender_name
                << ", interface=" << interface_name
                << ", path=" << obj_path << "} : " << status << std::endl;

        if (status.Check(StatusMajor::CONNECTION, StatusMinor::CONN_DISCONNECTED))
        {
            if (main_loop)
            {
                g_main_loop_quit(main_loop);
            }
        }
    }

private:
    GMainLoop* main_loop = nullptr;
};


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <--config|--session-path> <config name>" << std::endl;
        return 1;
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    std::string mode{argv[1]};
    std::string session_path{};

    OpenVPN3SessionMgrProxy mgr(dbus);
    if ("--config" == mode)
    {
        std::vector<std::string> sp = mgr.LookupConfigName(argv[2]);
        if (sp.size() > 1)
        {
            std::cerr << "More than one session was found with '" << argv[2]
                      << "' as config name" << std::endl;
            return 2;
        }
        else if (sp.size() == 0)
        {
            std::cerr << "No sessions found with '" << argv[2]
                      << "' as config name" << std::endl;
            return 2;
        }
        session_path = sp[0];
    }
    else if ("--session-path" == mode)
    {
        session_path = std::string(argv[2]);
    }
    else
    {
        std::cerr << "** ERROR **  Need --config or --session-path" << std::endl;
        return 2;
    }

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    std::cout << "Attaching to session path: " << session_path << std::endl;
    try
    {
        auto logsub = LogFwdSubscription::create(main_loop, dbus, session_path);
        std::cout << "Log level: " << std::to_string(logsub->GetLogLevel()) << std::endl;
        std::cout << "D-Bus bus name: " << dbus.GetUniqueBusName() << std::endl;
        g_main_loop_run(main_loop);
    }
    catch (const DBusProxyAccessDeniedException& e)
    {
        std::cerr << "** ERROR ** " << e.what() << std::endl;
        std::cerr << "            " << e.getDebug() << std::endl;
        return 1;
    }

    return 0;
}
