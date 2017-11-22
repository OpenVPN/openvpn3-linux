//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN, Inc. <davids@openvpn.net>
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

#ifndef OPENVPN3_DBUS_PROXY_CONFIG_HPP
#define OPENVPN3_DBUS_PROXY_CONFIG_HPP

#include "dbus/core.hpp"

using namespace openvpn;

class OpenVPN3ConfigurationProxy : public DBusProxy {
public:
    OpenVPN3ConfigurationProxy(GBusType bus_type, std::string target)
        : DBusProxy(bus_type,
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "", true)
    {
        std::string object_path = get_object_path(bus_type, target);
        proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                           OpenVPN3DBus_interf_configuration,
                           object_path);
        property_proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                                    "org.freedesktop.DBus.Properties",
                                    object_path);
    }

    OpenVPN3ConfigurationProxy(DBus const & dbusobj, std::string target)
        : DBusProxy(dbusobj,
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "", true)
    {
        std::string object_path = get_object_path(GetBusType(), target);
        proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                           OpenVPN3DBus_interf_configuration,
                           object_path);
        property_proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                                    "org.freedesktop.DBus.Properties",
                                    object_path);
    }

    std::string Import(std::string name, std::string config_blob,
                       bool single_use, bool persistent)
    {
        GVariant *res = Call("Import",
                             g_variant_new("(ssbb)",
                                           name.c_str(),
                                           config_blob.c_str(),
                                           single_use,
                                           persistent));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to import configuration");
        }

        gchar *buf = NULL;
        g_variant_get(res, "(o)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }

    std::string GetJSONConfig()
    {
        GVariant *res = Call("FetchJSON");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to retrieve configuration (JSON format)");
        }

        gchar *buf = NULL;
        g_variant_get(res, "(s)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }

    std::string GetConfig()
    {
        GVariant *res = Call("Fetch");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy", "Failed to retrieve configuration");
        }

        gchar *buf = NULL;
        g_variant_get(res, "(s)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }

    void SetAlias(std::string aliasname)
    {
        SetProperty("alias", aliasname);
    }

    void Seal()
    {
        GVariant *res = Call("Seal");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to seal the configuration");
        }
    }

private:
    std::string get_object_path(const GBusType bus_type, std::string target)
    {
        if (target[0] != '/')
        {
            // If the object path does not start with a leadning slash (/),
            // it is an alias, so we need to query /net/openvpn/v3/configuration/aliases
            // to retrieve the proper configuration path
            DBusProxy alias_proxy(bus_type,
                                  OpenVPN3DBus_name_configuration,
                                  OpenVPN3DBus_interf_configuration,
                                  OpenVPN3DBus_rootp_configuration + "/aliases/" + target);
            return alias_proxy.GetStringProperty("config_path");
        }
        else
        {
            return target;
        }
    }

};

#endif // OPENVPN3_DBUS_PROXY_CONFIG_HPP
