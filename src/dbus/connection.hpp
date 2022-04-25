//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   connection.hpp
 *
 * @brief  Main D-Bus connection handling
 *
 */
#pragma once

#include <iostream>

#include <gio/gio.h>

#include "exceptions.hpp"
#include "idlecheck.hpp"


/**
 *  This is the D-Bus connector object.  This acquires the requested
 *  well known bus name on either G_BUS_TYPE_SYSTEM (system bus) or
 *  G_BUS_TYPE_SESSION (session bus).
 *
 *  It is intended that the callback_bus_acquired() method registers
 *  the first D-Bus object (by subclassing the DBusObject class).
 *
 *  Once this object have been instantiated, the Setup() method must
 *  be called to start interacting with the D-Bus.
 */
class DBus
{
public:
    DBus(GBusType bustype)
        : keep_connection(false),
          bus_type(bustype),
          connected(false),
          connection_only(true),
          setup_complete(false),
          dbuscon(nullptr)
    {
        idle_checker = nullptr;
    }


    DBus(GDBusConnection *dbuscon)
        : keep_connection(true),
          connected(false),
          connection_only(true),
          setup_complete(true),
          dbuscon(dbuscon)
    {
        idle_checker = nullptr;
        connected = g_dbus_connection_is_closed(dbuscon) == 0;
    }


    DBus(GBusType bustype, std::string busname, std::string root_path, std::string default_interface )
        : keep_connection(false),
          idle_checker(nullptr),
          bus_type(bustype),
          connected(false),
          connection_only(false),
          setup_complete(false),
          busname(busname),
          root_path(root_path),
          default_interface(default_interface),
          dbuscon(nullptr)
    {
    }

    DBus(GDBusConnection *dbuscon, std::string busname, std::string root_path, std::string default_interface )
        : keep_connection(true),
          idle_checker(nullptr),
          connected(false),
          connection_only(false),
          setup_complete(false),
          busname(busname),
          root_path(root_path),
          default_interface(default_interface),
          dbuscon(dbuscon)
    {
        idle_checker = nullptr;
        connected = g_dbus_connection_is_closed(dbuscon) == 0;
    }

    virtual ~DBus()
    {
        close_and_cleanup();
    }


    /**
     *  Open a connection to the D-Bus infrastructure
     */
    void Connect()
    {
        if (connected)
        {
            return;
        }

        // Get D-Bus connection
        GError *error = NULL;
        dbuscon = g_bus_get_sync(bus_type, NULL, &error);
        if (!dbuscon || error)
        {
            std::string errmsg = "Could not connect to the D-Bus daemon for " + busname;
            if (error)
            {
                errmsg += ": " + std::string(error->message);
            }
            THROW_DBUSEXCEPTION("DBus", errmsg);
        }
        connected = true;
    }


    /**
     *  Enables signalling which will shutdown the connection and the
     *  program if it has been idling too long.  It is the IdleCheck
     *  object which decides what is idle.
     *
     * @param chk  Smart pointer to an IdleCheck object which tracks the
     *             idle state
     */
    void EnableIdleCheck(IdleCheck::Ptr& chk) noexcept
    {
        idle_checker = chk.get();
    }


    /**
     *  Configures and prepares to connect to the D-Bus infrastructure
     */
    void Setup()
    {
        if (!connected)
        {
            Connect();
        }

        if (connection_only)
        {
            THROW_DBUSEXCEPTION("DBus", "DBus object not prepared for owning bus name. Use the proper DBus constructor");
        }

        if (setup_complete)
        {
            THROW_DBUSEXCEPTION("DBus", "D-Bus setup already completed.");
        }

        // Acquire the requested bus name
        busid = g_bus_own_name_on_connection(dbuscon,
                                             busname.c_str(),
                                             G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                             int_callback_name_acquired,
                                             int_callback_name_lost,
                                             this,
                                             NULL);
        if (busid < 1)
        {
            THROW_DBUSEXCEPTION("DBus", "Could not own bus name for " + busname);
        }
        setup_complete = true;
        callback_bus_acquired();
    }


    /**
     *  Get the D-Bus bus type this connection object is configured for
     *
     * @return GBusType of the configured bus type.
     */
    GBusType GetBusType() const noexcept
    {
        return bus_type;
    }


    /**
     *  Get a glib2/gio GDBusConnection pointer to the established
     *  connection.
     *
     * @return  GDBUsConnection pointer to the currently extablished
     *          D-Bus.  In case of errors, an exception will be thrown.
     */
    GDBusConnection * GetConnection() const
    {
        if (!connected || !G_IS_DBUS_CONNECTION(dbuscon))
        {
            THROW_DBUSEXCEPTION("DBus", "D-Bus setup incomplete.  Missing Connect() call?");
        }
        if (g_dbus_connection_is_closed(dbuscon))
        {
            THROW_DBUSEXCEPTION("DBus", "Connection lost");
        }
        return dbuscon;
    }


    /**
     *   Return the numeric bus identification for this connection
     *
     * @return  Returns an unsigned integer referencing the current bus ID.
     *          In case of errors, an exception is thrown.
     */
    guint GetBusID() const
    {
        if (!connected || !G_IS_DBUS_CONNECTION(dbuscon))
        {
            THROW_DBUSEXCEPTION("DBus", "D-Bus setup incomplete.  Missing Setup() call?");
        }
        return busid;
    }


    /**
     *   Retrieve the configured busname for this connection.  This can
     *   only be called on connections which is configured with a bus name.
     *   For connections-only setups, calling this method is an error.
     *
     * @return  Returns a std::string containing the configured D-Bus
     *          bus name.  In case of errors an exception is thrown.
     */
    std::string GetBusName() const
    {
        if (connection_only)
        {
            THROW_DBUSEXCEPTION("DBus", "DBus object not prepared for owning bus name. Use the proper DBus constructor");
        }
        return busname;
    }


    const std::string GetUniqueBusName() const
    {
        if (!connected || !G_IS_DBUS_CONNECTION(dbuscon))
        {
            THROW_DBUSEXCEPTION("DBus", "D-Bus connection not established");
        }
        return std::string(g_dbus_connection_get_unique_name(dbuscon));
    }


    /**
     *   Retrieve the configured root object path for this connection.
     *   This can only be called on connections which is configured with a
     *   bus name. For connections-only setups, calling this method is an
     *   error.
     *
     * @return  Returns a std::string containing the configured D-Bus
     *          root object path.  In case of errors an exception is
     *          thrown.
     */
    std::string GetRootPath() const
    {
        if (connection_only)
        {
            THROW_DBUSEXCEPTION("DBus", "DBus object not prepared for owning bus name. Use the proper DBus constructor");
        }
        return root_path;
    }


    /**
     *   Retrieve the configured default interface to use for this
     *   connection and D-Bus object.
     *
     *   This can only be called on connections which is configured with a
     *   bus name. For connections-only setups, calling this method is an
     *   error.
     *
     * @return  Returns a std::string containing the configured default
     *          D-Bus interface for this connection.  In case of errors an
     *          exception is thrown.
     */
    std::string GetDefaultInterface() const
    {
        if (connection_only)
        {
            THROW_DBUSEXCEPTION("DBus", "DBus object not prepared for owning bus name. Use the proper DBus constructor");
        }
        return default_interface;
    }


    /**
     * Called when the D-Bus connection have been successfully acquired.
     */
    virtual void callback_bus_acquired()
    {
        // This cannot be a pure virtual method, as D-Bus clients don't
        // require to own a bus name
    }


    /**
     *  Called when the bus name have been successfully acquired.
     */
    virtual void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
        // This cannot be a pure virtual method, as D-Bus clients don't
        // require to own a bus name
    }


    /**
     *  Called if the bus name could not be acquired.  Either this or @callback_bus_acquired
     *  will be called.
     */
    virtual void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        // This cannot be a pure virtual method, as D-Bus clients are
        // not required to own a bus name
    }


protected:
    bool keep_connection;  /**< Do not disconnect when DBus connection object is removed */
    IdleCheck *idle_checker;

    void close_and_cleanup() noexcept
    {
        // If this object is based on an existing D-Bus connection,
        // don't disconnect.
        if (keep_connection)
        {
            return;
        }

        if (!connection_only
            && (busid > 0)
            && dbuscon
            && G_IS_DBUS_CONNECTION(dbuscon))
        {
            g_bus_unown_name(busid);
            GError *err = NULL;
            g_dbus_connection_close_sync(dbuscon, NULL, &err);
            if (err)
            {
                std::cout << "** ERROR ** D-Bus disconnect failed:"
                          << err->message
                          << std::endl;
                g_error_free(err);
            }
        }
        if (dbuscon && G_IS_OBJECT(dbuscon))
        {
            g_object_unref(dbuscon);
        }
    }

private:
    GBusType bus_type = GBusType::G_BUS_TYPE_NONE;
    bool connected;
    bool connection_only;
    bool setup_complete;
    std::string busname;
    std::string root_path;
    std::string default_interface;
    GDBusConnection *dbuscon;
    guint busid = 0;

    /**
     *  C wrapper function for the GDBus g_bus_own_name_on_connection()
     *  callback function.  This is called once the D-Bus library managed
     *  to retrieve the requested bus name.
     *
     * @param conn      D-Bus connection pointer calling this function
     * @param name      char * containing the bus name aqcuired
     * @param this_ptr  void * to the current object.  This *must* point
     *                  at a valid DBus object which handles this bus-name.
     */
    static void int_callback_name_acquired(GDBusConnection *conn, const gchar *name, gpointer this_ptr)
    {
        class DBus *obj = (class DBus *) this_ptr;
        obj->callback_name_acquired(conn, name);
    }


    /**
     *  C wrapper function for the GDBus g_bus_own_name_on_connection()
     *  callback function.  This is called once the D-Bus library managed
     *  to retrieve the requested bus name.
     *
     * @param conn      D-Bus connection pointer calling this function
     * @param name      char * containing the bus name aqcuired
     * @param this_ptr  void * to the current object.  This *must* point
     *                  at a valid DBus object which handles this bus-name.
     */
    static void int_callback_name_lost(GDBusConnection *conn, const gchar *name, gpointer this_ptr)
    {
        class DBus *obj = (class DBus *) this_ptr;
        obj->callback_name_lost(conn, name);
    }
};
