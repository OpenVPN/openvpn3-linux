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
 * @file   signal-listener.cpp
 *
 * @brief  Simple DBusSignalSubscription test utility.  Subscribes
 *         to either all or a filtered set of signals and prints them
 *         to stdout.  If the signal type is StatusChange, ProcessChange
 *         or AttentionRequired, it will decode that information as well.
 */

#include <iostream>
#include <string.h>

#include <glib-unix.h>

#include "dbus/core.hpp"
#include "common/utils.hpp"
#include "log/log-helpers.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "sessionmgr/sessionmgr-events.hpp"

using namespace openvpn;


class SigSubscription : public DBusSignalSubscription
{
public:
    SigSubscription(DBus& dbusobj,
                    const std::string& bus_name,
                    const std::string& interface,
                    const std::string& object_path,
                    const std::string& signal_name)
       : DBusSignalSubscription(dbusobj, bus_name, interface,
                                object_path, signal_name)
    {
    }


    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters)
    {
        // Filter out NetworkManager related signals - they happen often
        // and can be a bit too disturbing
        if (object_path.find("/org/freedesktop/NetworkManager/") != std::string::npos)
        {
            //std::cout << "NM signal ignored" << std::endl;
            return;
        }

        // Filter out certain non-OpenVPN 3 related signal interfaces
        if (interface_name.find("org.freedesktop.systemd1") != std::string::npos
            || interface_name.find("org.freedesktop.DBus.ObjectManager") != std::string::npos
            || interface_name.find("org.freedesktop.login1.Manager") != std::string::npos
            || interface_name.find("org.freedesktop.PolicyKit1.Authority") != std::string::npos)
        {
            return;
        }

        if ((signal_name == "NameOwnerChanged")
            || (signal_name == "PropertiesChanged"))
        {
            // Ignore these signals.  We don't need them and they're noise for us
        }
        else if (signal_name == "StatusChange")
        {
            guint major, minor;
            gchar *msg = nullptr;
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
        else if (signal_name =="ProcessChange")
        {
            guint minor;
            gchar *procname = nullptr;
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
        else if (signal_name == "AttentionRequired")
        {
            guint type;
            guint group;
            gchar *message = nullptr;
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
        else if (signal_name == "Log")
        {
            guint group = 0;
            guint catg = 0;
            gchar *message = nullptr;
            gchar *sesstok = nullptr;

            std::string typestr{g_variant_get_type_string(parameters)};
            if ("(uus)" == typestr)
            {
                g_variant_get (parameters, "(uus)", &group, &catg, &message);
            }
            else if ("(uuss)" == typestr)
            {
                g_variant_get (parameters, "(uuss)", &group, &catg,
                               &sesstok, &message);
            }
            else
            {
                std::cout << "-- Log: { UNKOWN FORMAT: " << typestr << "}" << std::endl;
                return;
            }
            std::cout << "-- Log: "
                      << "sender=" << sender_name
                      << ", interface=" << interface_name
                      << ", path=" << object_path
                      << (sesstok ? ", session_token=" + std::string(sesstok) : "")
                      << ": [" << std::to_string(group) << ", "
                      << std::to_string(catg) << "] "
                      << "-- type=" << LogGroup_str[group] << ", "
                      << "group=" << LogCategory_str[catg] << ", "
                      << " message='" << std::string(message) << "'"
                      << std::endl;
        }
        else if (signal_name == "NetworkChange")
        {
            NetCfgChangeEvent ev(parameters);

            std::cout << "-- NetworkChange: "
                      << "sender=" << sender_name
                      << ", interface=" << interface_name
                      << ", path=" << object_path
                      << ": [" << std::to_string((std::uint8_t) ev.type)
                      << "] " << ev
                      << std::endl;
        }
        else if (signal_name == "SessionManagerEvent")
        {
            SessionManager::Event ev(parameters);

            std::cout << "-- SessionManagerEvent: "
                            << "[sender=" << sender_name
                            << ", interface=" << interface_name
                            << ", path=" << object_path
                            << "] " << ev
                            << std::endl;
        }
        else
        {
            std::cout << "** Signal received: "
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
    std::string sig_name = (argc > 1 ? argv[1] : "");
    std::string interf   = (argc > 2 ? argv[2] : "");
    std::string obj_path = (argc > 3 ? argv[3] : "");
    std::string bus_name = (argc > 4 ? argv[4] : "");

    try
    {
        DBus dbus(G_BUS_TYPE_SYSTEM);
        dbus.Connect();
        std::cout << "Connected to D-Bus" << std::endl;

        SigSubscription subscription(dbus, bus_name, interf,
                                           obj_path, sig_name);

        std::cout << "Subscribed" << std::endl
                  << "Bus name:    " << (bus_name.empty() ? bus_name : "(not set)") << std::endl
                  << "Interface:   " << (interf.empty() ? interf : "(not set)") << std::endl
                  << "Object path: " << (obj_path.empty() ? obj_path : "(not set)") << std::endl
                  << "Signal name: " << (sig_name.empty() ? sig_name : "(not set)") << std::endl;

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
        g_unix_signal_add(SIGINT, stop_handler, main_loop);
        g_unix_signal_add(SIGTERM, stop_handler, main_loop);
        g_main_loop_run(main_loop);
    }
    catch (const DBusException& excp)
    {
        std::cerr << "EXCEPTION: " << excp.what() << std::endl;
        return 2;
    }
    return 0;
}
