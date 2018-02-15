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

#ifndef OPENVPN3_DBUS_PROXY_HPP
#define OPENVPN3_DBUS_PROXY_HPP

namespace openvpn
{
    class DBusProxy : public DBus
    {
    public:
        DBusProxy(GBusType bus_type,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath)
            : DBus(bus_type),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            proxy = SetupProxy(bus_name, interface, object_path);
            property_proxy = SetupProxy(bus_name,
                                        "org.freedesktop.DBus.Properties",
                                        object_path);
        }


        DBusProxy(GBusType bus_type,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath,
                  bool hold_setup_proxy)
            : DBus(bus_type),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            if (!hold_setup_proxy)
            {
                proxy = SetupProxy(bus_name, interface, object_path);
                property_proxy = SetupProxy(bus_name,
                                            "org.freedesktop.DBus.Properties",
                                            object_path);
            }
        }


        DBusProxy(GDBusConnection *dbusconn,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath)
            : DBus(dbusconn),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            proxy = SetupProxy(bus_name, interface, object_path);
            property_proxy = SetupProxy(bus_name,
                                        "org.freedesktop.DBus.Properties",
                                        object_path);
        }


        DBusProxy(GDBusConnection *dbusconn,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath,
                  bool hold_setup_proxy)
            : DBus(dbusconn),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            if (!hold_setup_proxy)
            {
                proxy = SetupProxy(bus_name, interface, object_path);
                property_proxy = SetupProxy(bus_name,
                                            "org.freedesktop.DBus.Properties",
                                            object_path);
            }
        }


        DBusProxy(DBus const & dbusobj,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath)
            : DBus(dbusobj),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            proxy = SetupProxy(bus_name, interface, object_path);
            property_proxy = SetupProxy(bus_name,
                                        "org.freedesktop.DBus.Properties",
                                        object_path);
        }


        DBusProxy(DBus const & dbusobj,
                  std::string const & busname,
                  std::string const & interf,
                  std::string const & objpath,
                  bool hold_setup_proxy)
            : DBus(dbusobj),
              proxy(nullptr),
              property_proxy(nullptr),
              bus_name(busname),
              interface(interf),
              object_path(objpath),
              call_flags(G_DBUS_CALL_FLAGS_NONE),
              proxy_init(false),
              property_proxy_init(false)
        {
            if( !hold_setup_proxy )
            {
                proxy = SetupProxy(bus_name, interface, object_path);
                property_proxy = SetupProxy(bus_name,
                                            "org.freedesktop.DBus.Properties",
                                            object_path);
            }
        }


        virtual ~DBusProxy()
        {
            // If this object is using an existing connection;
            // don't trigger a disconnect.  This variable is
            // defined and set in the DBus class.
            if (keep_connection)
            {
                return;
            }

            if (proxy_init)
            {
                g_object_unref(proxy);
            }

            if (property_proxy_init)
            {
                g_object_unref(property_proxy);
            }
        }


        void SetGDBusCallFlags(GDBusCallFlags flags)
        {
            call_flags = flags;
        }


        GVariant * Call(std::string method, GVariant *params, bool noresponse = false)
        {
            if (method.empty())
            {
                THROW_DBUSEXCEPTION("DBusProxy", "Method cannot be empty");
            }

            GError *error = NULL;
            if (!noresponse)
            {
                // Where we care about the response, we use a synchronous call
                // and wait for the response
                GVariant *ret = g_dbus_proxy_call_sync(proxy,
                                                       method.c_str(),
                                                       params,      // parameters to method
                                                       call_flags,
                                                       -1,          // timeout, -1 == default
                                                       NULL,        // GCancellable
                                                       &error);
                if (!ret || error)
                {
                    std::stringstream errmsg;
                    errmsg << "Failed calling D-Bus method " << method << ": "
                           << error->message;
                    THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
                }
                return ret;
            }
            else
            {
                g_dbus_proxy_call(proxy, method.c_str(), params,
                                  call_flags,
                                  -1,       // timeout, -1 == default
                                  NULL,     // GCancellable
                                  NULL,     // Response callback, not needed here
                                  NULL);    // user_data, not needed due to no callback
                return NULL;
            }
        }


        GVariant * Call(std::string method, bool noresponse = false)
        {
            return Call(method, NULL, noresponse);
        }


        GVariant * GetProperty(std::string property)
        {
            if (property.empty())
            {
                THROW_DBUSEXCEPTION("DBusProxy", "Property cannot be empty");
            }

            // Use the org.freedesktop.DBus.Properties.Get() method directly
            // instead of going via a list of cached properties.  The cache
            // might not be updated and we get the wrong values.

            GError *error = NULL;
            GVariant *response = g_dbus_proxy_call_sync(property_proxy,
                                                        "Get",
                                                        g_variant_new("(ss)",
                                                                      interface.c_str(),
                                                                      property.c_str()),
                                                        G_DBUS_CALL_FLAGS_NONE,
                                                        -1,          // timeout, -1 == default
                                                        NULL,        // GCancellable
                                                        &error);
            if (!response || error)
            {
                std::stringstream errmsg;
                errmsg << "Failed calling D-Bus method "
                       << "org.freedesktop.DBus.Properties.Get("
                       << "interface=" << interface
                       << ", property=" << property
                       << ")";
                if (error)
                {
                    errmsg << ": " << error->message;
                }
                THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
            }
            GVariant * ret = NULL;
            g_variant_get(response, "(v)", &ret);
            g_variant_unref(response);
            return ret;
        }


        bool GetBoolProperty(std::string property)
        {
            GVariant *res = GetProperty(property);
            bool ret = g_variant_get_boolean(res);
            g_variant_unref(res);
            return ret;
        }


        std::string GetStringProperty(std::string property)
        {
            gsize len = 0;
            GVariant *res = GetProperty(property);
            std::string ret = std::string(g_variant_get_string(res, &len));
            g_variant_unref(res);
            return ret;
        }


        guint32 GetUIntProperty(std::string property)
        {
            GVariant *res = GetProperty(property);
            guint32 ret = g_variant_get_uint32(res);
            g_variant_unref(res);
            return ret;
        }


        guint64 GetUInt64Property(std::string property)
        {
            GVariant *res = GetProperty(property);
            guint64 ret = g_variant_get_uint64(res);
            g_variant_unref(res);
            return ret;
        }


        void SetProperty(std::string property, GVariant *value)
        {
            if (property.empty())
            {
                THROW_DBUSEXCEPTION("DBusProxy", "Property cannot be empty");
            }

            // NOTE:
            // It is tempting to consider using g_dbus_proxy_set_cached_property()
            // instead calling org.freedesktop.DBus.Properties.Set() on its own
            // proxy connection.  But that only updates the local cache, the
            // change is never sent to the backend service.

            GError *error = NULL;
            GVariant *ret = g_dbus_proxy_call_sync(property_proxy,
                                                   "Set",
                                                   g_variant_new("(ssv)",
                                                                 interface.c_str(),
                                                                 property.c_str(),
                                                                 value),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,          // timeout, -1 == default
                                                   NULL,        // GCancellable
                                                   &error);
            if (!ret || error)
            {
                std::stringstream errmsg;
                errmsg << "Failed calling D-Bus method org.freedesktop.DBus.Properties.Set()";
                if (error)
                {
                    errmsg << ": " << error->message;
                }
                THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
            }
            g_variant_unref(ret);
        }


        inline void SetProperty(std::string property, bool value)
        {
            SetProperty(property, g_variant_new_boolean(value));
        }


        inline void SetProperty(std::string property, std::string value)
        {
            SetProperty(property, g_variant_new_string(value.c_str()));
        }


        inline void SetProperty(std::string property, guint32 value)
        {
            SetProperty(property, g_variant_new_uint32(value));
        }


    protected:
        GDBusProxy *proxy;
        GDBusProxy *property_proxy;

        GDBusProxy * SetupProxy(std::string busn, std::string intf, std::string objp)
        {
            if (busn.empty()) {
                THROW_DBUSEXCEPTION("DBusProxy", "Bus name cannot be empty");
            }

            if (intf.empty()) {
                THROW_DBUSEXCEPTION("DBusProxy", "Interface cannot be empty");
            }

            if (objp.empty()) {
                THROW_DBUSEXCEPTION("DBusProxy", "Object path cannot be empty");
            }

            // Connect do the D-Bus without a bus name
            // This is safe to call, multiple times - as DBus::Connect()
            // checks if a connection is already established
            Connect();

            /*
              std::cout << "[DBusProxy::SetupProxy] bus_name=" << busn
                      << ", interface=" << intf
                      << ", object_path=" << objp
                      << std::endl;
            */

            // Prepare a new D-Bus proxy, which the
            // client side uses when communicating with
            // a D-Bus service
            GError *error = NULL;
            GDBusProxy *retprx = g_dbus_proxy_new_sync(GetConnection(),
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                       NULL,             // GDBusInterfaceInfo
                                                       busn.c_str(),     // aka. destination
                                                       objp.c_str(),
                                                       intf.c_str(),
                                                       NULL,             // GCancellable
                                                       &error);
            if (!retprx || error)
            {
                std::stringstream errmsg;
                errmsg << "Failed preparing proxy";
                if (error)
                {
                    errmsg << ": " << error->message;
                }
                THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
            }
            if ("org.freedesktop.DBus.Properties" == intf)
            {
                property_proxy_init = true;
            }
            else
            {
                proxy_init = true;
            }
            return retprx;
        }


        GDBusProxy * SetupProxy()
        {
            return SetupProxy(bus_name, interface, object_path);
        }


    private:
        std::string bus_name;
        std::string interface;
        std::string object_path;
        GDBusCallFlags call_flags;
        bool proxy_init;
        bool property_proxy_init;
    };
};
#endif // OPENVPN3_DBUS_PROXY_HPP
