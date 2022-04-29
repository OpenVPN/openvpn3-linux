//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2022  Arne Schwabe <arne@openvpn.net>
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

#pragma once

#include <memory>
#include <gio/gunixfdlist.h>

#ifdef HAVE_TINYXML
#include <openvpn/common/exception.hpp>
#include <openvpn/common/xmlhelper.hpp>

// using XmlDocPtr = std::shared_ptr<openvpn::Xml::Document>;
typedef std::shared_ptr<openvpn::Xml::Document> XmlDocPtr;
#endif


#include "dbus/connection.hpp"
#include "glibutils.hpp"

/// Default timeout for D-Bus calls.  Setting this to -1 uses the default
/// value in glib2, which is typically 20 seconds.  We reduce it to 5 seconds
#define DBUS_PROXY_CALL_TIMEOUT 5000


class DBusProxyAccessDeniedException: std::exception
{
public:
    DBusProxyAccessDeniedException(const std::string& method,
                          const std::string& debug)
        : debug(debug)
    {
        std::stringstream err;
        err << "Access denied to " << method;
        error = err.str();
    }

    virtual const char* what() const noexcept
    {
        return error.c_str();
    }


    virtual const char* getDebug() const noexcept
    {
        return debug.c_str();
    }


private:
    std::string error;
    std::string debug;
};


class DBusProxy : public DBus
{
public:
    DBusProxy(GBusType bus_type,
              std::string busname,
              std::string interf,
              std::string objpath)
        : DBus(bus_type),
          proxy(nullptr),
          property_proxy(nullptr),
          bus_name(std::move(busname)),
          interface(std::move(interf)),
          object_path(std::move(objpath)),
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

    /**
     *  Retrieve the object path this proxy is accessing
     *
     * @return  Returns a std::string of the D-Bus object path
     *          this proxy is using.
     */
    std::string GetProxyPath() const
    {
        return object_path;
    }


    void SetGDBusCallFlags(GDBusCallFlags flags)
    {
        call_flags = flags;
    }


#ifdef HAVE_TINYXML
    XmlDocPtr Introspect()
    {
        GDBusProxy* introsprx = SetupProxy(bus_name,
                                          "org.freedesktop.DBus.Introspectable",
                                          object_path);

        GVariant* res = dbus_proxy_call(introsprx, "Introspect", nullptr,
                                        false, G_DBUS_CALL_FLAGS_NONE);
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to call introspect method");
        }

        XmlDocPtr doc;
        doc.reset(new Xml::Document(GLibUtils::ExtractValue<std::string>(res, 0),
                                    "introspect"));
        g_variant_unref(res);

        return doc;
    }
#endif

    /**
     *  Some service expose a 'version' property in the main manager
     *  object.  This retrieves this but has a retry logic in case the
     *  service did not start up quickly enough.
     *
     * @return  Returns a string containing the version of the service
     *
     */
    std::string GetServiceVersion()
    {
        int delay = 1;
        for (int attempts = 10; attempts > 0; --attempts)
        {
            try
            {
                return GetStringProperty("version");
            }
            catch(DBusProxyAccessDeniedException& excp)
            {
                return std::string(""); // Consider this an unknown version
            }
            catch (DBusException& excp)
            {
                std::string err(excp.what());
                if (err.find("No such interface 'org.freedesktop.DBus.Properties' on object") == std::string::npos)
                {
                    if ((err.find(": No such property 'version'") != std::string::npos))
                    {
                        return std::string("");  // Consider this as an unknown version but not an error
                    }
                }
                sleep(delay);
                ++delay;
            }
            catch (...)
            {
                throw;
            }
        }
        THROW_DBUSEXCEPTION("DBusProxy",
                            "Could not establish a connection with "
                            "'" + bus_name + "'");
    }


    /**
     *  Checks if the destination service is available by checking if
     *  the service bus name is registered.  If not, try to start the
     *  service.
     *
     *  Throws DBusException in case of errors.
     */
    void CheckServiceAvail()
    {
        GDBusProxy *proxy = SetupProxy("org.freedesktop.DBus",
                                       "org.freedesktop.DBus",
                                           "/");
        for (int i = 5; i > 0; --i)
        {
            try
            {
                GVariant *r = dbus_proxy_call(proxy,
                                              "StartServiceByName",
                                              g_variant_new("(su)",
                                                            bus_name.c_str(),
                                                            0 // Not in use by D-Bus
                                                            ),
                                               false,
                                               G_DBUS_CALL_FLAGS_NONE);
                if (r)
                {
                    GLibUtils::checkParams(__func__, r, "(u)", 1);
                    guint res = GLibUtils::ExtractValue<unsigned int>(r, 0);
                    g_variant_unref(r);

                    if (2 == res) // DBUS_START_REPLY_ALREADY_RUNNING
                    {
                        // When getting a clear evidence the service
                        // is already running, no need to bother more
                        return;
                    }

                    // FIXME:  Try to find better ways to
                    //         identify availability in  a reasonable
                    //         way.  Perhaps calling
                    //         org.freedesktop.DBus.GetNameOwner coupled
                    //         with CheckObjectExists()?  Using Ping()
                    //         now for the initial testing
                    Ping();
                    return;
                }
            }
            catch (DBusException& excp)
            {
                if (1 == i)
                {
                    THROW_DBUSEXCEPTION("DBusProxy",
                                        "D-Bus service '"
                                        + bus_name + "' did not start");
                }
                sleep(1);
            }
        }
    }


    /**
     *  Checks if the object being accessed is really available.
     *
     *  This is done by a crude hack which just checks if the
     *  net.freedesktop.DBus.Properties.GetAll() method works.  If it
     *  does, it is presumed the object path is valid and points to an
     *  existing D-Bus object.
     *
     * @param  allow_tries   How many times it should do an attempt
     *                       before calling it a failure.  This is 1
     *                       by default but can be overridden by caller
     *                       if needs to wait for a service to start up
     *                       and settle.
     * @param  sleep_us      How long (in µs) it should wait between
     *                       each attempt.  Default: 1000
     *
     * @return  Returns true if the object exists or false if not.  Will
     *          throw an exception if the the D-Bus method calls fails or
     *          the property proxy interface has not been configured.
     */
    bool CheckObjectExists(const unsigned int allow_tries=1,
                           const unsigned int sleep_us=1000)
    {
        if (!property_proxy_init)
        {
            THROW_DBUSEXCEPTION("DBusProxy",
                                "Property proxy has not been initialized");
        }

        unsigned int attempts = allow_tries;
        while (0 < attempts)
        {
            try
            {
                // Objects will normally have the
                // org.freedesktop.DBus.Properties.GetAll() method available,
                // so if this fails we presume the object does not exist.
                GVariant *empty = dbus_proxy_call(property_proxy,
                                                  "GetAll",
                                                  g_variant_new("(s)", interface.c_str()),
                                                  false, call_flags);
                if (empty)
                {
                    g_variant_unref(empty);
                }
                return true;
            }
            catch (DBusProxyAccessDeniedException& excp)
            {
                // This is fine in this case, it means we don't
                // have access to all properties which again means the
                // object must exist.
                return true;
            }
            catch (DBusException& excp)
            {
                std::string err(excp.what());
                if ((err.find("Name \"") != std::string::npos)
                    && err.find("\" does not exist") != std::string::npos)
                {
                    return false;
                }

                --attempts;
                if (0 == attempts)
                {
                    return false;
                }
                else
                {
                    usleep(sleep_us);
                }
            }
        }
        return false;
    }


    /**
     *  Tries to ping a the destination service.  This is used to
     *  activate auto-start of services and give it time to settle.
     *  It will try 3 times with a sleep of 1 second in between.
     *
     *  If it does not repsond after three attempts, it will
     *  throw a DBusException.
     */
    void Ping()
    {
        GDBusProxy *peer_proxy = SetupProxy(bus_name,
                                           "org.freedesktop.DBus.Peer",
                                           "/");

        for (int i=0; i < 3; i++)
        {
            try
            {
                // The Ping() request does not give any response, but
                // we want to make this call synchronous and wait for
                // this call to truly have happened.  Then we just
                // throw away the empty response, to avoid a memleak.
                GVariant *empty = dbus_proxy_call(peer_proxy, "Ping",
                                                  NULL, false, call_flags);
                if (empty)
                {
                    g_variant_unref(empty);
                }
                usleep(400); // Add some additional gracetime
                return;
            }
            catch (DBusException& excp)
            {
                if (2 == i)
                {
                    THROW_DBUSEXCEPTION("DBusProxy",
                                        "D-Bus service '"
                                        + bus_name + "' did not respond");
                }
                sleep(1);
            }
        }
    }


    std::string GetNameOwner(const std::string& name)
    {
        GDBusProxy *proxy = SetupProxy("org.freedesktop.DBus",
                                       "org.freedesktop.DBus",
                                           "/");
        GVariant *r = dbus_proxy_call(proxy,
                                      "GetNameOwner",
                                      g_variant_new("(s)", name.c_str()),
                                      false,
                                      G_DBUS_CALL_FLAGS_NONE);
        if (r)
        {
            GLibUtils::checkParams(__func__, r, "(s)", 1);
            std::string res(GLibUtils::ExtractValue<std::string>(r, 0));
            g_variant_unref(r);
            return res;
        }
        THROW_DBUSEXCEPTION("GetNameOwner", "No owner found");
    }


    int StartServiceByName(const std::string& name)
    {
        GDBusProxy *proxy = SetupProxy("org.freedesktop.DBus",
                                       "org.freedesktop.DBus",
                                           "/");
        GVariant *r = dbus_proxy_call(proxy,
                                      "StartServiceByName",
                                      g_variant_new("(su)",
                                                    name.c_str(),
                                                    0 // Not in use by D-Bus
                                                    ),
                                       false,
                                       G_DBUS_CALL_FLAGS_NONE);
        if (r)
        {
            GLibUtils::checkParams(__func__, r, "(u)", 1);
            guint res = GLibUtils::ExtractValue<unsigned int>(r, 0);
            g_variant_unref(r);
            return res;
        }
        THROW_DBUSEXCEPTION("StartServiceByName",
                            std::string("Failed requesting starting of ")
                            + "service '" + name + "' ");
    }


    GVariant * Call(std::string method, GVariant *params, bool noresponse = false) const
    {
        return dbus_proxy_call(proxy, method, params, noresponse,
                               call_flags);
    }


    GVariant * Call(std::string method, bool noresponse = false) const
    {
        return dbus_proxy_call(proxy, method, NULL, noresponse,
                               call_flags);
    }

    GVariant * CallGetFD(std::string method, int& fd, bool noresponse = false) const
    {
        return dbus_proxy_call(proxy, method, NULL, noresponse,
                               call_flags, &fd);
    }

    /**
     * Will send an additional fd in addition to the normal function call.
     * This method will *not* take ownership of the fd. The caller needs
     * to close the fd if it is not needed anymore
     */
    GVariant * CallSendFD(std::string method, GVariant* params, int fd, bool noresponse = false) const
    {
        return dbus_proxy_call(proxy, method, params, noresponse,
                               call_flags, nullptr, fd);
    }


    GVariant * GetProperty(std::string property) const
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
                                                    DBUS_PROXY_CALL_TIMEOUT,
                                                    NULL,        // GCancellable
                                                    &error);
        if (!response && !error)
        {
            THROW_DBUSEXCEPTION("DBusProxy", "Unspecified error");
        }
        else if (!response && error)
        {
            std::string dbuserr(error->message);

            if (dbuserr.find("GDBus.Error:org.freedesktop.DBus.Error.AccessDenied:") != std::string::npos)
            {
                throw DBusProxyAccessDeniedException(property + " property",
                                                     dbuserr);
            }

            g_dbus_error_strip_remote_error(error);
            std::stringstream errmsg;
            errmsg << "Failed retrieving property value for "
                   << "'" << property << "': " << error->message;
            g_error_free(error);
            THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
        }

        GVariant* chld = g_variant_get_child_value(response, 0);
        GVariant* ret = g_variant_get_variant(chld);
        g_variant_unref(chld);
        g_variant_unref(response);
        return ret;
    }


    bool GetBoolProperty(std::string property) const
    {
        GVariant *res = GetProperty(property);
        bool ret = g_variant_get_boolean(res);
        g_variant_unref(res);
        return ret;
    }


    std::string GetStringProperty(std::string property) const
    {
        gsize len = 0;
        GVariant *res = GetProperty(property);
        std::string ret = std::string(g_variant_get_string(res, &len));
        g_variant_unref(res);
        return ret;
    }


    guint32 GetUIntProperty(std::string property) const
    {
        GVariant *res = GetProperty(property);
        guint32 ret = g_variant_get_uint32(res);
        g_variant_unref(res);
        return ret;
    }


    guint64 GetUInt64Property(std::string property) const
    {
        GVariant *res = GetProperty(property);
        guint64 ret = g_variant_get_uint64(res);
        g_variant_unref(res);
        return ret;
    }


    void SetProperty(std::string property, GVariant *value) const
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
                                               DBUS_PROXY_CALL_TIMEOUT,
                                               NULL,        // GCancellable
                                               &error);
        if (!ret && !error)
        {
            THROW_DBUSEXCEPTION("DBusProxy", "Unspecified error");
        }
        else if (!ret && error)
        {
            std::string dbuserr(error->message);

            if (dbuserr.find("GDBus.Error:org.freedesktop.DBus.Error.AccessDenied:") != std::string::npos)
            {
                throw DBusProxyAccessDeniedException(property + " property",
                                                     dbuserr);
            }

            g_dbus_error_strip_remote_error(error);
            std::stringstream errmsg;
            errmsg << "Failed setting new property value on "
                   << "'" << property << "': " << error->message;
            g_error_free(error);
            THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
        }            g_variant_unref(ret);
    }


    inline void SetProperty(std::string property, bool value) const
    {
        SetProperty(property, g_variant_new_boolean(value));
    }


    inline void SetProperty(std::string property, std::string value) const
    {
        SetProperty(property, g_variant_new_string(value.c_str()));
    }


    inline void SetProperty(std::string property, guint32 value) const
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

        if (!g_variant_is_object_path(objp.c_str()))
        {
            THROW_DBUSEXCEPTION("DBusProxy", "Invalid D-Bus path");
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
                g_dbus_error_strip_remote_error(error);
                errmsg << ": " << error->message;
            }
            g_error_free(error);
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

    // Note we only implement single fd out/in for the fd API since that
    // is all we currently need and handling fd extraction here makes
    // error handling easier
    GVariant * dbus_proxy_call(GDBusProxy *prx, std::string method,
                               GVariant *params, bool noresponse,
                               GDBusCallFlags flags,
                               int *fd_out = nullptr,
                               int fd_in = -1) const
    {
        if (method.empty())
        {
            THROW_DBUSEXCEPTION("DBusProxy", "Method cannot be empty");
        }

        // Ensure we still have a valid connection
        (void) GetConnection();

        GError *error = nullptr;
        GVariant *ret = nullptr;
        GUnixFDList *out_fdlist = nullptr;
        if (!noresponse)
        {
            // Where we care about the response, we use a synchronous call
            // and wait for the response
            if (!fd_out && fd_in == -1)
            {
                ret = g_dbus_proxy_call_sync(prx,
                                             method.c_str(),
                                             params,      // parameters to method
                                             flags,
                                             DBUS_PROXY_CALL_TIMEOUT,
                                             nullptr,        // GCancellable
                                             &error);
            }
            else
            {
                /* Default these pointers to their "not used" value */
                GUnixFDList *fdlist = nullptr;
                GUnixFDList** out_fdlist_ptr = nullptr;

                if (fd_in >=0)
                {
                    fdlist = g_unix_fd_list_new();
                    g_unix_fd_list_append(fdlist, fd_in, &error);
                }
                if (fd_out)
                {
                    out_fdlist_ptr = &out_fdlist;
                }
                if (!error)
                {
                    ret = g_dbus_proxy_call_with_unix_fd_list_sync(prx,
                                                                   method.c_str(),
                                                                   params,      // parameters to method
                                                                   flags,
                                                                   DBUS_PROXY_CALL_TIMEOUT,
                                                                   fdlist,     // fd_list (to send)
                                                                   out_fdlist_ptr,
                                                                   nullptr,        // GCancellable
                                                                   &error);
                }
                if(ret && !error && fd_out)
                {
                    *fd_out = g_unix_fd_list_get(out_fdlist, 0, &error);
                    GLibUtils::unref_fdlist(out_fdlist);
                }
                if (fdlist)
                {
                    GLibUtils::unref_fdlist(fdlist);
                }
            }
            if (!ret && !error)
            {
                THROW_DBUSEXCEPTION("DBusProxy", "Unspecified error");
            }
            else if (!ret && error)
            {
                std::string dbuserr(error->message);

                if ((dbuserr.find("GDBus.Error:org.freedesktop.DBus.Error.AccessDenied:") != std::string::npos)
                    || (dbuserr.find("GDBus.Error:net.openvpn.v3.error.acl.denied:") != std::string::npos))
                {
                    throw DBusProxyAccessDeniedException(method, dbuserr);
                }

                g_dbus_error_strip_remote_error(error);
                std::stringstream errmsg;
                errmsg << "Failed calling D-Bus method " << method << ": "
                       << error->message;
                g_error_free(error);
                THROW_DBUSEXCEPTION("DBusProxy", errmsg.str());
            }
            return ret;
        }
        else
        {
            g_dbus_proxy_call(prx, method.c_str(), params,
                              flags,
                              DBUS_PROXY_CALL_TIMEOUT,
                              nullptr,     // GCancellable
                              nullptr,     // Response callback, not needed here
                              nullptr);    // user_data, not needed due to no callback
            return nullptr;
        }
    }
};
