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

#pragma once

#include <map>
#include <vector>

#include "core.hpp"

// Several Glib2 functions used cannot use empty strings if the argument
// to the function is to be ignored, it must be NULL.  Since we need this
// behaviour more places, we define it as a macro.
#define STRING_TO_CHARPTR(x) x.empty() ? nullptr : x.c_str()

class DBusSignalSubscription
{
public:
    // FIXME:  Reduce number of constructors
    //         Move towards passing these variables to Subscribe()
    //         as the normal way to subscribe

    DBusSignalSubscription(DBus & dbusobj,
                           std::string interf)
        : conn(dbusobj.GetConnection()),
          interface(interf)
    {
    }


    DBusSignalSubscription(DBus & dbusobj,
                           std::string busname,
                           std::string interf,
                           std::string objpath)
        : conn(dbusobj.GetConnection()),
          bus_name(busname),
          interface(interf),
          object_path(objpath)
    {
    }


    DBusSignalSubscription(GDBusConnection *dbuscon,
                           std::string interf)
        : conn(dbuscon),
          interface(interf)
    {
    }


    DBusSignalSubscription(GDBusConnection *dbuscon,
                           std::string busname,
                           std::string interf,
                           std::string objpath)
        : conn(dbuscon),
          bus_name(busname),
          interface(interf),
          object_path(objpath)
    {
    }


    DBusSignalSubscription(DBus & dbusobj,
                           std::string busname,
                           std::string interf,
                           std::string objpath,
                           std::string signame)
        : conn(dbusobj.GetConnection()),
          bus_name(busname),
          interface(interf),
          object_path(objpath)
    {
        Subscribe(signame);
    }


    DBusSignalSubscription(GDBusConnection *dbuscon,
                           std::string busname,
                           std::string interf,
                           std::string objpath,
                           std::string signame)
        : conn(dbuscon),
          bus_name(busname),
          interface(interf),
          object_path(objpath)
    {
        Subscribe(signame);
    }


    /**
     *  If another destructor overrides this one,
     *  the caller *MUST* run Cleanup() before exiting
     *  to properly unsibscribe from the D-Bus signal
     */
    virtual ~DBusSignalSubscription()
    {
        Cleanup();
    }

    /**
     *  Called each time the subscribed signal has a match on the D-Bus
     */
    virtual void callback_signal_handler(GDBusConnection *connection,
                                         const std::string sender_name,
                                         const std::string object_path,
                                         const std::string interface_name,
                                         const std::string signal_name,
                                         GVariant *parameters) = 0;


    void Subscribe(std::string busname, std::string objpath, std::string signal_name)
    {
        guint signal_id = g_dbus_connection_signal_subscribe(conn,
                                                       STRING_TO_CHARPTR(busname),
                                                       STRING_TO_CHARPTR(interface),
                                                       STRING_TO_CHARPTR(signal_name),
                                                       STRING_TO_CHARPTR(objpath),
                                                       NULL,
                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                       dbusobject_callback_signal_handler,
                                                       this,
                                                       NULL );  // destructor
        if (signal_id == 0)
        {
            std::stringstream err;
            err << "Failed to subscribe to the " << signal_name << "signal on"
                << object_path << "[" << interface << "]";
            THROW_DBUSEXCEPTION("DBusSignalSubscription", err.str());
        }
        subscriptions[signal_name] = signal_id;
        subscribed = true;
    }


    void Subscribe(std::string objpath, std::string signal_name)
    {
        Subscribe(bus_name, objpath, signal_name);
    }


    void Subscribe(std::string signal_name)
    {
        Subscribe(object_path, signal_name);
    }


    void Unsubscribe(std::string signal_name)
    {
        if (subscriptions[signal_name] > 0)
        {
            g_dbus_connection_signal_unsubscribe(conn, subscriptions[signal_name]);
            subscriptions[signal_name] = 0;
        }
    }


    std::string GetBusName()
    {
        return bus_name;
    }


    std::string GetInterface()
    {
        return interface;
    }


    std::string GetObjectPath()
    {
        return object_path;
    }


    guint GetSignalId(std::string signal_name)
    {
        return subscriptions[signal_name];
    }


    void Cleanup()
    {
        if( !subscribed )
        {
            return;
        }
        if (!G_IS_DBUS_CONNECTION(conn))
        {
            return;
        }
        for( auto& sub : subscriptions)
        {
            if (sub.second > 0)
            {
                g_dbus_connection_signal_unsubscribe(conn, sub.second);
            }
            subscriptions[sub.first] = 0;
        }
        subscribed = false;
    }


protected:
    GDBusConnection * GetConnection()
    {
        return conn;
    }


    static void dbusobject_callback_signal_handler(GDBusConnection *conn,
                                                    const gchar *sender,
                                                    const gchar *obj_path,
                                                    const gchar *intf_name,
                                                    const gchar *sign_name,
                                                    GVariant *params,
                                                    gpointer this_ptr)
    {
        class DBusSignalSubscription *obj = (class DBusSignalSubscription *) this_ptr;
        obj->callback_signal_handler(conn,
                                     std::string(sender),
                                     std::string(obj_path),
                                     std::string(intf_name),
                                     std::string(sign_name),
                                     params);
    }


private:
    GDBusConnection *conn;
    std::string bus_name;
    std::string interface;
    std::string object_path;
    std::map<std::string, guint> subscriptions;
    bool subscribed = false;
};



class DBusSignalProducer
{
public:
    DBusSignalProducer(DBus dbuscon,
                       std::string busname,
                       std::string interf,
                       std::string objpath) :
        conn(dbuscon.GetConnection()),
        interface(interf),
        object_path(objpath)
    {
        if (!busname.empty())
        {
            target_bus_names.push_back(busname);
        }
        validate_params();
    }


    DBusSignalProducer(GDBusConnection *con,
                       std::string busname,
                       std::string interf,
                       std::string objpath) :
        conn(con),
        interface(interf),
        object_path(objpath)
    {
        if (!busname.empty())
        {
            target_bus_names.push_back(busname);
        }
        validate_params();
    }


    void AddTargetBusName(const std::string busn)
    {
        target_bus_names.push_back(busn);
    }


    void Send(const std::vector<std::string>& bus_names,
              const std::string& interf,
              const std::string& objpath,
              const std::string& signal_name,
              GVariant *params) const
    {
        g_variant_ref_sink(params);  // This method must own this object

        for (const auto& target : bus_names)
        {
            send_signal(target, interf, objpath, signal_name, params);
        }

        g_variant_unref(params);  // Now params can be released and freed

    }


    void Broadcast(const std::string interf,
                   const std::string objpath,
                   const std::string signal_name,
                   GVariant *params) const
    {
        g_variant_ref_sink(params);
        send_signal("", interf, objpath, signal_name, params);
    }


    void Send(const std::string busn,
                     const std::string interf,
                     const std::string objpath,
                     const std::string signal_name,
                     GVariant *params) const
    {
        g_variant_ref_sink(params);  // This method must own this object

        if (!busn.empty() || 0 == target_bus_names.size())
        {
            send_signal(busn, interf, objpath, signal_name, params);
        }

        if (0 < target_bus_names.size())
        {
            Send(target_bus_names, interf, objpath, signal_name, params);
        }

        g_variant_unref(params);  // Now params can be released and freed
    }


    void Send(const std::string busn,
              const std::string interf,
              const std::string signal_name,
              GVariant *params) const
    {
        Send(busn, interf, object_path, signal_name, params);
    }


    void Send(const std::string busn, const std::string interf, const std::string signal_name) const
    {
        Send(busn, interf, object_path, signal_name, NULL);
    }


    void Send(const std::string interf, const std::string signal_name, GVariant *params) const
    {
        Send("", interf, object_path, signal_name, params);
    }


    void Send(const std::string interf, const std::string signal_name) const
    {
        Send("", interf, object_path, signal_name, NULL);
    }


    void Send(const std::string signal_name, GVariant *params) const
    {
        Send("", interface, object_path, signal_name, params);
    }


    void SendTarget(const std::string& target, const std::string& signame,
                    GVariant *params) const
    {
        Send(target, interface, object_path, signame, params);
    }

    void Send(const std::string signal_name) const
    {
        Send("", interface, object_path, signal_name, NULL);
    }


protected:
    void set_object_path(const std::string& new_path)
    {
        object_path = new_path;
    }

    const std::string get_object_path() const
    {
        return object_path;
    }

    const std::string get_interface() const
    {
        return interface;
    }

    void validate_params()
    {
        if (interface.empty()) {
            THROW_DBUSEXCEPTION("DBusSignalProducer", "Interface cannot be empty");
        }

        if (object_path.empty()) {
            THROW_DBUSEXCEPTION("DBusSignalProducer", "Object path cannot be empty");
        }
    }


private:
    GDBusConnection *conn;
    std::vector<std::string> target_bus_names;
    std::string interface;
    std::string object_path;
    std::string signal_name;

    void send_signal(const std::string busn,
                     const std::string interf,
                     const std::string objpath,
                     const std::string signal_name,
                     GVariant *params) const
    {
        /*
          std::cout << "Signal Send: bus=" << (!target_bus_names.empty() ? target_bus_names : "(not set)")
                  << ", interface=" << (!interface.empty() ? interface : "(not set)")
                  << ", object_path=" << (!object_path.empty() ? object_path : "(not set)")
                  << ", signal_name=" << signal_name
                  << std::endl;
         */

        if (NULL == g_dbus_connection_get_unique_name(conn))
        {
            THROW_DBUSEXCEPTION("DBusSignalProducer",
                                "D-Bus connection invalid");
        }

        GError *error = NULL;
        if( !g_dbus_connection_emit_signal(conn,
                                           STRING_TO_CHARPTR(busn),
                                           STRING_TO_CHARPTR(objpath),
                                           STRING_TO_CHARPTR(interf),
                                           signal_name.c_str(),
                                           params,
                                           &error))
        {
            std::stringstream errmsg;
            errmsg << "Failed to send '" + signal_name + "' signal";

            if (error)
            {
                errmsg << ": " << error->message;
            }
            THROW_DBUSEXCEPTION("DBusSignalProducer", errmsg.str());
        }
    }
};
