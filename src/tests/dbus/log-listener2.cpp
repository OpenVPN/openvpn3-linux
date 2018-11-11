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
 * @file   log-listener2.cpp
 *
 * @brief  Subscribes to any log signals sent from either the session manager
 *         or a backend service and prints them to stdout.  This uses a
 *         more advanced approach using the LogConsumer class
 */

#include <iostream>
#include <string.h>

#include <glib-unix.h>

#include "dbus/core.hpp"
#include "log/dbus-log.hpp"
#include "common/utils.hpp"

using namespace openvpn;

class Logger : public LogConsumer
{
public:
    Logger(GDBusConnection *dbuscon, std::string tag, std::string interf)
        : LogConsumer(dbuscon, interf, ""),
          log_tag(tag)
    {
    }

    void ConsumeLogEvent(const std::string sender, const std::string interface,
                         const std::string object_path,
                         const LogEvent& logev)
    {
        std::cout << log_tag << ":: sender=" << sender
                  << ", interface=" << interface
                  << " path=" << object_path << std::endl
                  << "       " << logev << std::endl;
    }

private:
    std::string log_tag;
};


int main(int argc, char **argv)
{
    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    std::cout << "Connected to D-Bus" << std::endl;

    Logger be_subscription(dbus.GetConnection(), "[B]", OpenVPN3DBus_interf_backends);
    Logger session_subscr(dbus.GetConnection(), "[S]", OpenVPN3DBus_interf_sessions);
    Logger config_subscr(dbus.GetConnection(), "[C]", OpenVPN3DBus_interf_configuration);

    std::cout << "Subscribed" << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);
    g_main_loop_run(main_loop);

    return 0;
}
