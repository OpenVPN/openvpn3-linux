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
 * @file   log-listener.cpp
 *
 * @brief  Subscribes to any log signals sent from either the session manager
 *         or a backend service and prints them to stdout.  This uses a
 *         simpler and more low-level approach via LogSubscription.
 */

#include <iostream>
#include <string.h>

#include <glib-unix.h>

#include "dbus/core.hpp"
#include "common/utils.hpp"

using namespace openvpn;

class LogSubscription : public DBusSignalSubscription
{
public:
    LogSubscription(DBus& dbusobj, std::string logtag, std::string interface)
        : DBusSignalSubscription(dbusobj,
                                 "",
                                 interface,
                                 "",
                                 "Log"),
          log_tag(logtag)
    {
    }


    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters)
    {
            guint group;
            guint logflags;
            gchar *msg = nullptr;
            g_variant_get (parameters, "(uus)", &group, &logflags, &msg);

            std::cout << log_tag << " Log entry (" << sender_name << ") interface=" << interface_name
                      << ", path=" << object_path << " : "
                      << "[" << group << ", " << logflags << "] "
                      << msg << std::endl;
    }

private:
    std::string log_tag;
};


int main(int argc, char **argv)
{
    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    std::cout << "Connected to D-Bus" << std::endl;

    LogSubscription be_subscription(dbus, "Backend", OpenVPN3DBus_interf_backends);
    LogSubscription session_subscr(dbus, "Session", OpenVPN3DBus_interf_sessions);

    std::cout << "Subscribed" << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);
    g_main_loop_run(main_loop);

    return 0;
}
