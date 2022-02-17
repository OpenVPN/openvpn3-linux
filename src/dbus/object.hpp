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

#include "idlecheck.hpp"

/**
 *  DBusObject is the object which carries data, methods
 *  and signals to be provided over the D-Bus.
 *
 *  Each DBusObject must have a unique object path
 *  and the introspection XML describes what this object
 *  contains.
 *
 *  The callback methods are called by the D-Bus framework
 *  and happens each time a client tries to do something
 *  with this object over D-Bus
 *
 *  The design idea with DBusObject is that it is primarily
 *  the D-Bus bus which owns and have the reference to a
 *  DBusObject instance.  So when a DBusObject is to be
 *  removed that should happen through deleting the object
 *  via the GDBus API and not by just deleting the object
 *  instance directly in C++.  This ensures that the object
 *  is not seen as available on any of the busses in D-Bus.
 *
 *  This DBusObject class is intended to be inheritted by
 *  a child class and only to implement the really needed
 *  virtual methods below.
 *
 */
class DBusObject
{
public:
    DBusObject(std::string obj_path, std::string introspection_xml) :
        registered(false),
        object_path(obj_path),
        object_id(0),
        idle_checker(nullptr)
    {
        ParseIntrospectionXML(introspection_xml);
    }


    DBusObject(std::string obj_path) :
        registered(false),
        object_path(obj_path),
        object_id(0),
        idle_checker(nullptr),
        introspection(nullptr)
    {
    }


    virtual ~DBusObject()
    {
        if (introspection)
        {
            g_dbus_node_info_unref(introspection);
        }
    }


    guint GetObjectId() const
    {
        if (!registered)
        {
            THROW_DBUSEXCEPTION("DBusObject", "Object have not been registered to D-Bus yet");
        }
        return object_id;
    }


    std::string GetObjectPath() const
    {
        return object_path;
    }


    void RegisterObject(GDBusConnection *dbuscon)
    {
        if (registered)
        {
            THROW_DBUSEXCEPTION("DBusObject", "Object is already registered in D-Bus");
        }
        if (nullptr == introspection)
        {
            THROW_DBUSEXCEPTION("DBusObject", "No introspection document parsed");
        }

        GError *error = NULL;
        object_id = g_dbus_connection_register_object(dbuscon,
                                                      object_path.c_str(),
                                                      introspection->interfaces[0],
                                                      &dbusobj_interface_vtable,
                                                      this,
                                                      NULL, // destruct function
                                                      &error);
        if (object_id < 1)
        {
            std::stringstream err;
            err << "RegisterObject(" + object_path + ") failed: ";
            err << (error != NULL ? error->message : "(unknown)");
            THROW_DBUSEXCEPTION("DBusObject", err.str());
        }
        registered = true;
    }


    /**
     *  Sets/registers an IdleChecker object for this DBusObject
     *
     *  @@param chk  A valid pointer to an IdleCheck object
     */
    void IdleCheck_Register(IdleCheck *chk)
    {
        idle_checker = chk;
    }


    void RemoveObject(GDBusConnection *dbuscon)
    {
        if (!registered)
        {
            THROW_DBUSEXCEPTION("DBusObject", "Object have not been registered to D-Bus yet");
        }
        registered = false;

        GError *err = nullptr;
        if (!g_dbus_connection_flush_sync(dbuscon, NULL, &err))
        {
            std::cout << "** ERROR ** Connection flush failed when "
                      << "removing object ["
                      << introspection->interfaces[0] << ":"
                      << object_path << "]:" << err->message
                      << std::endl;
        }

        // Remove the object from the D-Bus
        g_dbus_connection_unregister_object(dbuscon, object_id);

        // Allow the implementor to add more cleaning up
        callback_destructor();

        // Remove the introspection document from memory
        g_dbus_node_info_unref(introspection);
        introspection = nullptr;
    }


    /**
     *  Called each time a D-Bus client calls an object method
     */
    virtual void callback_method_call(GDBusConnection *conn,
                                      const std::string sender,
                                      const std::string obj_path,
                                      const std::string intf_name,
                                      const std::string meth_name,
                                      GVariant *params,
                                      GDBusMethodInvocation *invoc) = 0;


    GVariant * _dbus_get_property_internal(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GError **error)
    {
        try
        {
            return callback_get_property(conn, sender, obj_path,
                                         intf_name, property_name, error);
        }
        catch (DBusPropertyException& err)
        {
            err.SetDBusError(error);
            return NULL;
        }
    }


    /**
     *  Called each time a D-Bus client attempts to read a D-Bus object property
     */
    virtual GVariant * callback_get_property(GDBusConnection *conn,
                                             const std::string sender,
                                             const std::string obj_path,
                                             const std::string intf_name,
                                             const std::string property_name,
                                             GError **error) = 0;


    /**
     *  Called each time a D-Bus client attempts to modify a D-Bus object property.
     *  This method is "private" and is the one called by the GDBus library.  This
     *  function calls the virtual callback_set_property() which needs to be
     *  implemented by the user.  Once that returns, we need to send a
     *  signal to D-Bus that a propery changed.
     */
    gboolean _dbus_set_property_internal(GDBusConnection *conn,
                                         const gchar *sender,
                                         const gchar *obj_path,
                                         const gchar *intf_name,
                                         const gchar *property_name,
                                         GVariant *value,
                                         GError **error)
    {
        try
        {
            GVariantBuilder *ret = callback_set_property(conn,
                                                         std::string(sender),
                                                         std::string(obj_path),
                                                         std::string(intf_name),
                                                         std::string(property_name),
                                                         value,
                                                         error);

            // If ret != NULL, we have a valid response which contains
            // information about what has changed.  This is further
            // used to issue a standard D-Bus signal that an object property
            // have been modified; which is the signal being emitted below.
            if (NULL != ret)
            {
                GError *local_err = NULL;
                g_dbus_connection_emit_signal (conn,
                                               NULL,
                                               obj_path,
                                               "org.freedesktop.DBus.Properties",
                                               "PropertiesChanged",
                                               g_variant_new ("(sa{sv}as)",
                                                              intf_name,
                                                              ret,
                                                              NULL),
                                               &local_err);
                g_variant_builder_unref(ret);

                if (local_err)
                {
                    std::stringstream err;
                    err << "callback_set_property(interface=" << intf_name
                        << ", path=" << object_path
                        << ", property=" << property_name
                        << ", value='" << value << "'"
                        << ") failed: "
                        << local_err->message;
                    THROW_DBUSEXCEPTION("DBusObject", err.str());
                }
            }
            else
            {
                if (NULL != *error)
                {
                    // FIXME: Consider if these errors should just go to
                    //        the client or if we should send and empty
                    //        response back
                    std::stringstream err;
                    err << "callback_set_property(interface=" << intf_name
                        << ", path=" << object_path
                        << ", property=" << property_name
                        << ", value='" << value << "'"
                        << ") failed: "
                        << (*error)->message;
                    THROW_DBUSEXCEPTION("DBusObject", err.str());
                }
            }

            return (NULL != ret);
        }
        catch (DBusPropertyException& err)
        {
            err.SetDBusError(error);
            return false;
        }
    }


    /**
     *  Called each time a D-Bus client attempts to modify a D-Bus object property
     */
    virtual GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GVariant *value,
                                           GError **error) = 0;


    /**
     *  Simple helper wrapper preparing the signal response needed by call_set_property()
     *  This prepares the response packet which is sent as a signal to D-Bus about which
     *  property was changed.
     *
     *  This is the string value variant
     *
     *  @param property  String containing the changed property
     *  @param value     String containing the new value
     *
     *  @return Returns a GVariantBuilder pointer which is to be consumed by
     *          _dbus_set_property_internal()
     *
     */
    GVariantBuilder * build_set_property_response(std::string property, std::string value)
    {
        GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add (builder,
                               "{sv}",
                               property.c_str(),
                               g_variant_new_string (value.c_str()));
        return builder;
    }

    /**
     *  Simple helper wrapper preparing the signal response needed by call_set_property()
     *  This prepares the response packet which is sent as a signal to D-Bus about which
     *  property was changed.
     *
     *  This is the boolean value variant
     *
     *  @param property  String containing the changed property
     *  @param value     String containing the new value
     *
     *  @return Returns a GVariantBuilder pointer which is to be consumed by
     *          _dbus_set_property_internal()
     *
     */
    GVariantBuilder * build_set_property_response(std::string property, const gboolean value)
    {
        GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add (builder,
                               "{sv}",
                               property.c_str(),
                               g_variant_new_boolean (value));
        return builder;
    }


    /**
     *  Simple helper wrapper preparing the signal response needed by call_set_property()
     *  This prepares the response packet which is sent as a signal to D-Bus about which
     *  property was changed.
     *
     *  This is the uint value variant
     *
     *  @param property  String containing the changed property
     *  @param value     String containing the new value
     *
     *  @return Returns a GVariantBuilder pointer which is to be consumed by
     *          _dbus_set_property_internal()
     *
     */
    GVariantBuilder * build_set_property_response(std::string property, const guint value)
    {
        GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add(builder,
                              "{sv}",
                              property.c_str(),
                              g_variant_new_uint32(value));
        return builder;
    }


    /**
     *  Simple helper wrapper preparing the signal response needed by call_set_property()
     *  This prepares the response packet which is sent as a signal to D-Bus about which
     *  property was changed.
     *
     *  This is the uint64 value variant
     *
     *  @param property  String containing the changed property
     *  @param value     unit64 containing the new value
     *
     *  @return Returns a GVariantBuilder pointer which is to be consumed by
     *          _dbus_set_property_internal()
     *
     */
    GVariantBuilder * build_set_property_response(std::string property, const uint64_t value)
    {
        GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add(builder,
                              "{sv}",
                              property.c_str(),
                              g_variant_new_uint64(value));
        return builder;
    }


    /**
     *  Simple helper wrapper preparing the signal response needed by call_set_property()
     *  This prepares the response packet which is sent as a signal to D-Bus about which
     *  property was changed.
     *
     *  This is a std::time_t to uint64_t value wrapper
     *
     *  @param property  String containing the changed property
     *  @param value     std::time_t containing the new value
     *
     *  @return Returns a GVariantBuilder pointer which is to be consumed by
     *          _dbus_set_property_internal()
     *
     */
    GVariantBuilder * build_set_property_response(std::string property, const std::time_t value)
    {
        return build_set_property_response(property, (uint64_t) value);
    }


    /**
     *  This destructor is optional and may be used by implementors to clean up
     *  before this object is deleted from both the D-Bus bus and memory.  This
     *  function is called from RemoveObject() right after the object is unregistered
     *  from the D-Bus.
     */
    virtual void callback_destructor () {}

protected:
    /**
     *  Parses and processes the introspection XML document
     *  describing this object.  This is used when registering this object
     *  to the D-Bus bus.
     *
     *  @param xmlstr  std::string containing the introspection XML document to use
     */
    void ParseIntrospectionXML(std::string xmlstr)
    {
        if (registered)
        {
            THROW_DBUSEXCEPTION("DBusObject", "Object is already registered in D-Bus. "
                                "Cannot modify the introspection document.");
        }

        GError *error = nullptr;
        introspection = g_dbus_node_info_new_for_xml(xmlstr.c_str(), &error);
        if (NULL == introspection || NULL != error)
        {
            THROW_DBUSEXCEPTION("DBusObject", "Failed to parse introspection XML:" + std::string(error->message));
        }
    }


    /**
     *  Parses and processes the introspection XML document
     *  describing this object.  This is used when registering this object
     *  to the D-Bus bus.
     *
     *  @param xmlstr  std::stringstream containing the introspection XML document to use
     */
    void ParseIntrospectionXML(std::stringstream& xmlstr)
    {
        ParseIntrospectionXML(xmlstr.str());
    }


    /**
     *  Updates the IdleCheck timer's timestamp to indicate this object have been accessed.
     *  If the IdleCheck object times out, the process is stopped.
     */
    void IdleCheck_UpdateTimestamp()
    {
        if( idle_checker )
        {
            idle_checker->UpdateTimestamp();
        }
    }

    /**
     *  Get the object pointer to the registered IdleCheck object
     *
     *  @returns Returns a pointer to an IdleCheck object or nullptr if not set/registered
     */
    IdleCheck * IdleCheck_Get()
    {
        return idle_checker;
    }


    void IdleCheck_RefInc()
    {
        if (idle_checker)
        {
            idle_checker->RefCountInc();
        }
    }


    void IdleCheck_RefDec()
    {
        if (idle_checker)
        {
            idle_checker->RefCountDec();
        }
    }


private:
    bool registered;
    std::string object_path;
    guint object_id;
    IdleCheck *idle_checker;
    GDBusNodeInfo *introspection;

    /**
     *  Callback loook-up table for D-Bus
     */
    GDBusInterfaceVTable dbusobj_interface_vtable = {
        dbusobject_callback_method_call,
        dbusobject_callback_get_property,
        dbusobject_callback_set_property
    };


    static void dbusobject_callback_method_call(GDBusConnection *conn,
                                                 const gchar *sender,
                                                 const gchar *obj_path,
                                                 const gchar *intf_name,
                                                 const gchar *meth_name,
                                                 GVariant *params,
                                                 GDBusMethodInvocation *invoc,
                                                 gpointer this_ptr)
    {
        class DBusObject *obj = (class DBusObject *) this_ptr;
        obj->callback_method_call(conn,
                                  std::string(sender),
                                  std::string(obj_path),
                                  std::string(intf_name),
                                  std::string(meth_name),
                                  params, invoc);
    }


    static GVariant * dbusobject_callback_get_property(GDBusConnection *conn,
                                                       const gchar *sender,
                                                       const gchar *obj_path,
                                                       const gchar *intf_name,
                                                       const gchar *property_name,
                                                       GError **error,
                                                       gpointer this_ptr)
    {
        class DBusObject *obj = (class DBusObject *) this_ptr;
        return obj->_dbus_get_property_internal(conn,
                                                std::string(sender),
                                                std::string(obj_path),
                                                std::string(intf_name),
                                                std::string(property_name),
                                                error);
    }


    static gboolean dbusobject_callback_set_property(GDBusConnection *conn,
                                                     const gchar *sender,
                                                     const gchar *obj_path,
                                                     const gchar *intf_name,
                                                     const gchar *property_name,
                                                     GVariant *value,
                                                     GError **error,
                                                     gpointer this_ptr)
    {
        class DBusObject *obj = (class DBusObject *) this_ptr;
        return obj->_dbus_set_property_internal(conn, sender,
                                                obj_path, intf_name,
                                                property_name, value,
                                                error);
    }
};

