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
 * @file   fetch-config.cpp
 *
 * @brief  Dumps a specific configuration stored in the configuration manager.
 *         This calls the D-Bus methods provided by the configuration manager
 *         directly and parses the results here.
 */

#include <iostream>
#include <gio/gio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config obj path>" << std::endl;
        return 1;
    }

    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!conn || error)
    {
        std::cout << "** ERROR ** g_bus_get_sync(): " << error->message << std::endl;
        return 2;
    }

    error = NULL;
    GDBusProxy *p = g_dbus_proxy_new_sync(conn,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,                            // GDBusInterfaceInfo
                                          "net.openvpn.v3.configuration",  // name
                                          argv[1],                         // object path
                                          "net.openvpn.v3.configuration",  // interface name
                                          NULL,                            // GCancellable
                                          &error);
    if (!p || error)
    {
        std::cout << "** ERROR ** g_dbus_proxy_new_sync(): " << error->message << std::endl;
        return 2;
    }

    GVariant *res_v = g_dbus_proxy_get_cached_property (p, "valid");
    gboolean valid = g_variant_get_boolean(res_v);
    if (false == valid)
    {
        std::cout << "** ERROR ** Configuration is not valid" << std::endl;
        return 3;
    }
    g_variant_unref(res_v);

    res_v = g_dbus_proxy_get_cached_property (p, "readonly");
    gboolean readonly = g_variant_get_boolean(res_v);
    g_variant_unref(res_v);

    res_v = g_dbus_proxy_get_cached_property (p, "persistent");
    gboolean persistent = g_variant_get_boolean(res_v);
    g_variant_unref(res_v);

    res_v = g_dbus_proxy_get_cached_property (p, "single_use");
    gboolean single_use = g_variant_get_boolean(res_v);
    g_variant_unref(res_v);

    res_v = g_dbus_proxy_get_cached_property (p, "name");
    gsize cfgnamelen = 0;
    const gchar *cfgname = g_variant_get_string(res_v, &cfgnamelen);
    g_variant_unref(res_v);

    error = NULL;
    res_v = g_dbus_proxy_call_sync(p,
                                   "Fetch",                // method
                                   NULL,                   // parameters to method
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,                     // timeout, -1 == default
                                   NULL,                   // GCancellable
                                   &error);
    if (!res_v || error)
    {
        std::cout << "** ERROR ** g_dbus_proxy_call_sync(): " << error->message << std::endl;
        return 2;
    }
    gchar *conf_s = nullptr;
    g_variant_get(res_v, "(s)", &conf_s);
    std::string config(conf_s);
    g_variant_unref(res_v);
    g_free(conf_s);

    std::cout << "Configuration: " << std::endl;
    std::cout << "  - Name:       " << cfgname << std::endl;
    std::cout << "  - Read only:  " << (readonly ? "Yes" : "No") << std::endl;
    std::cout << "  - Persistent: " << (persistent ? "Yes" : "No") << std::endl;
    std::cout << "  - Usage:      " << (single_use ? "Once" : "Multiple times") << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << config << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    // Clean-up and disconnect
    g_object_unref(p);
    g_object_unref(conn);
    std::cout << "** DONE" << std::endl;
}
