//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

/**
 * @file   signal-listener.cpp
 *
 * @brief  Simple DBusSignalSubscription test utility.  Subscribes
 *         to either all or a filtered set of signals and prints them
 *         to stdout.  If the signal type is StatusChange, ProcessChange
 *         or AttentionRequired, it will decode that information as well.
 */

#include <iostream>
#include <string.h>

#include "dbus/core.hpp"
#include "common/utils.hpp"

using namespace openvpn;

class SigSubscription : public DBusSignalSubscription
{
public:
    SigSubscription(DBus& dbusobj,
                    std::string bus_name,
                    std::string interface,
                    std::string object_path,
                    std::string signal_name)
        : DBusSignalSubscription(dbusobj, bus_name, interface, object_path, signal_name)
    {
    }


    void callback_signal_handler(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters)
    {
        if (0 == g_strcmp0(signal_name, "StatusChange"))
        {
            guint major, minor;
            gchar *msg = NULL;
            g_variant_get (parameters, "(uus)", &major, &minor, &msg);

            std::cout << "-- Status Change: interface=" << interface_name
                      << ", path=" << object_path
                      << ": [" << std::to_string(major)
                      << ", " << std::to_string(minor) << "] "
                      << StatusMajor_str[major]
                      << " - " << StatusMinor_str[minor];
            if (msg && strlen(msg) > 0) {
                std::cout << ", " << msg;
            }
            std::cout << std::endl;
        }
        else if (0 == g_strcmp0(signal_name, "ProcessChange"))
        {
            guint minor;
            gchar *procname = NULL;
            guint pid;
            g_variant_get (parameters, "(usu)", &minor, &procname, &pid);

            std::cout << "-- Process Change: interface=" << interface_name
                      << ", path=" << object_path
                      << ": [" << std::to_string(minor) << "] "
                      << StatusMinor_str[minor]
                      << " -- Process name=" << std::string(procname)
                      << " (pid: " << std::to_string(pid) << ")"
                      << std::endl;
        }
        else if (0 == g_strcmp0(signal_name, "AttentionRequired"))
        {
            guint type;
            guint group;
            gchar *message = NULL;
            g_variant_get (parameters, "(uus)", &type, &group, &message);

            std::cout << "-- User Attention Required: "
                      << "sender=" << sender_name
                      << ", interface=" << interface_name
                      << ", path=" << object_path
                      << ": [" << std::to_string(type) << ", "
                      << std::to_string(group) << "] "
                      << "-- type=" << ClientAttentionType_str[type] << ", "
                      << "group=" << ClientAttentionGroup_str[group] << ", "
                      << " message='" << std::string(message) << "'"
                      << std::endl;
        }
        else
        {
            std::cout << "** Signal recieved: "
                      << ": sender=" << sender_name
                      << ", path=" << object_path
                      << ", interface=" << interface_name
                      << ", signal=" << signal_name
                      << std::endl;
        }
    }
};


int main(int argc, char **argv)
{
    const char *sig_name = (argc > 1 ? argv[1] : NULL);
    const char *interf   = (argc > 2 ? argv[2] : NULL);
    const char *obj_path = (argc > 3 ? argv[3] : NULL);
    const char *bus_name = (argc > 4 ? argv[4] : NULL);

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    std::cout << "Connected to D-Bus" << std::endl;

    SigSubscription subscription(dbus, C_char2string(bus_name), C_char2string(interf),
                                 C_char2string(obj_path), C_char2string(sig_name));

    std::cout << "Subscribed" << std::endl
              << "Bus name:    " << (bus_name != NULL ? bus_name : "(not set)") << std::endl
              << "Interface:   " << (interf != NULL ? interf : "(not set)") << std::endl
              << "Object path: " << (obj_path != NULL ? obj_path : "(not set)") << std::endl
              << "Signal name: " << (sig_name != NULL ? sig_name : "(not set)") << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);
    g_main_loop_run(main_loop);

    return 0;
}
