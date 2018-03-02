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
 * @file   enable-logging.cpp
 *
 * @brief  Simple tool to flip the logging flag on an on-going VPN session.
 *         This effectively just enables/disables the session manager to proxy
 *         log messages from the VPN client backend to any process subscribed
 *         to log messages from the session manager.
 */

#include <iostream>
#include <string>

#include "dbus/core.hpp"

using namespace openvpn;

int main(int argc, char **argv)
{
        if (argc < 3) {
                std::cout << "Usage: " << argv[0]
                          << " <enable/disable> <session path> [<loglevel]>"
                          << std::endl;
                return 1;
        }

        bool arg_enable = (0 == g_strcmp0("1", argv[1])
                           || 0 == g_strcmp0("yes", argv[1])
                           || 0 == g_strcmp0("true", argv[1])
                           || 0 == g_strcmp0("enable", argv[1])
                           || 0 == g_strcmp0("on", argv[1]));

        bool arg_disable = (0 == g_strcmp0("0", argv[1])
                            || 0 == g_strcmp0("no", argv[1])
                            || 0 == g_strcmp0("false", argv[1])
                            || 0 == g_strcmp0("disable", argv[1])
                            || 0 == g_strcmp0("off", argv[1]));

        if( !arg_enable && !arg_disable) {
                std::cerr << "** ERROR **  Invalid boolean flag. Must be 0/1, yes/no, "
                          << "enable/disable, true/false or on/off" << std::endl;
                return 2;
        }

        if( NULL == argv[2]) {
                std::cerr << "** ERROR **  No session object path provided." << std::endl;
                return 2;
        }

        std::cout << (arg_enable ? "Enabling" : "Disabling")
                  << " logging on session " << argv[2] << std::endl;


        try {
                DBusProxy proxy(G_BUS_TYPE_SYSTEM,
                                OpenVPN3DBus_name_sessions,
                                OpenVPN3DBus_interf_sessions,
                                argv[2]);


                try {
                        bool setting = proxy.GetBoolProperty("receive_log_events");
                        std::cout << "Current setting: " << (setting ? "enabled" : "disabled") << std::endl;
                        proxy.SetProperty("receive_log_events", arg_enable);
                        setting = proxy.GetBoolProperty("receive_log_events");
                        std::cout << "Changed to: " << (setting ? "enabled" : "disabled") << std::endl;

                        if( argc > 3) {
                                unsigned loglev = proxy.GetUIntProperty("log_verbosity");
                                std::cout << "Current log verbosity: " << std::to_string(loglev) << ::std::endl;
                                proxy.SetProperty("log_verbosity", (unsigned int) atoi(argv[3]));
                                loglev = proxy.GetUIntProperty("log_verbosity");
                                std::cout << "New log_verbosity: " << loglev << ::std::endl;
                        }

                } catch (DBusException& err) {
                        std::cerr << "** ERROR ** Failed reading/setting the logging property" << std::endl;
                        std::cerr << "-- DEBUG -- " << err.what() << std::endl;
                        return 3;
                }

        } catch (DBusException& err) {
                std::cerr << "** ERROR ** Failed to find the session.  Is the path correct?" << std::endl;
                std::cerr << "-- DEBUG -- " << err.what() << std::endl;
                return 3;
        }
        return 0;
}

