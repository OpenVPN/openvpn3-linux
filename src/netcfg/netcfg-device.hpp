//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-device.hpp
 *
 * @brief  D-Bus object representing a single virtual network device
 *         the net.openvpn.v3.netcfg service manages
 */

#pragma once

#include <openvpn/common/rc.hpp>

#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/glibutils.hpp"
#include "ovpn3cli/lookup.hpp"
#include "netcfg-stateevent.hpp"
#include "netcfg-signals.hpp"

enum class NetCfgDeviceType
{
    UNSET,   // Primarily to avoid 0 but still have 0 defined
    TUN,
    TAP
};

class NetCfgDevice : public DBusObject,
                     public DBusCredentials
{
public:
    NetCfgDevice(GDBusConnection *dbuscon,
                 std::function<void()> remove_callback,
                 const uid_t creator, const std::string& objpath,
                 const NetCfgDeviceType& devtype, const std::string& devname,
                 const unsigned int log_level, LogWriter *logwr)
        : DBusObject(objpath),
          DBusCredentials(dbuscon, creator),
          remove_callback(remove_callback),
          device_type(devtype),
          device_name(devname),
          signal(dbuscon, LogGroup::NETCFG, objpath, logwr)
    {
        signal.SetLogLevel(log_level);
        std::stringstream introspect;
        introspect << "<node name='" << objpath << "'>"
                   << "    <interface name='" << OpenVPN3DBus_interf_netcfg << "'>"
                   << "        <method name='AddIPv4Address'>"
                   << "            <arg direction='in' type='s' name='ip_address'/>"
                   << "            <arg direction='in' type='u' name='prefix'/>"
                   << "            <arg direction='in' type='s' name='broadcast'/>"
                   << "        </method>"
                   << "        <method name='RemoveIPv4Address'>"
                   << "            <arg direction='in' type='s' name='ip_address'/>"
                   << "            <arg direction='in' type='u' name='prefix'/>"
                   << "        </method>"
                   << "        <method name='AddIPv6Address'>"
                   << "            <arg direction='in' type='s' name='ip_address'/>"
                   << "            <arg direction='in' type='u' name='prefix'/>"
                   << "        </method>"
                   << "        <method name='RemoveIPv6Address'>"
                   << "            <arg direction='in' type='s' name='ip_address'/>"
                   << "            <arg direction='in' type='u' name='prefix'/>"
                   << "        </method>"
                   << "        <method name='AddRoutes'>"
                   << "            <arg direction='in' type='as' name='route_target'/>"
                   << "            <arg direction='in' type='s' name='gateway'/>"
                   << "        </method>"
                   << "        <method name='RemoveRoutes'>"
                   << "            <arg direction='in' type='as' name='route_target'/>"
                   << "            <arg direction='in' type='s' name='gateway'/>"
                   << "        </method>"
                   << "        <method name='AddDNS'>"
                   << "            <arg direction='in' type='as' name='server_list'/>"
                   << "        </method>"
                   << "        <method name='RemoveDNS'>"
                   << "            <arg direction='in' type='as' name='server_list'/>"
                   << "        </method>"
                   << "        <method name='AddDNSSearch'>"
                   << "            <arg direction='in' type='as' name='server_list'/>"
                   << "        </method>"
                   << "        <method name='RemoveDNSSearch'>"
                   << "            <arg direction='in' type='as' name='server_list'/>"
                   << "        </method>"
                   << "        <method name='Activate'/>"
                   << "        <method name='Disable'/>"
                   << "        <method name='Destroy'/>"
                   << "        <property type='u'  name='log_level' access='readwrite'/>"
                   << "        <property type='u'  name='owner' access='read'/>"
                   << "        <property type='au' name='acl' access='read'/>"
                   << "        <property type='u' name='device_type' access='read'/>"
                   << "        <property type='s' name='device_name' access='read'/>"
                   << "        <property type='b'  name='active' access='read'/>"
                   << "        <property type='as' name='ipv4_addresses' access='read'/>"
                   << "        <property type='as' name='ipv4_routes' access='read'/>"
                   << "        <property type='as' name='ipv6_addresses' access='read'/>"
                   << "        <property type='as' name='ipv6_routes' access='read'/>"
                   << "        <property type='as' name='dns_servers' access='read'/>"
                   << "        <property type='as' name='dns_search' access='read'/>"
                   << signal.GetLogIntrospection()
                   << NetCfgStateEvent::IntrospectionXML()
                   << "    </interface>"
                   << "</node>";
        ParseIntrospectionXML(introspect);
        signal.LogVerb2("Network device '" + devname + "' prepared");
    }

    ~NetCfgDevice()
    {
        remove_callback();
        IdleCheck_RefDec();
    }


    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this BackendClientObject.
     *
     * @param conn        D-Bus connection where the method call occurred
     * @param sender      D-Bus bus name of the sender of the method call
     * @param obj_path    D-Bus object path of the target object.
     * @param intf_name   D-Bus interface of the method call
     * @param method_name D-Bus method name to be executed
     * @param params      GVariant Glib2 object containing the arguments for
     *                    the method call
     * @param invoc       GDBusMethodInvocation where the response/result of
     *                    the method call will be returned.
     */
    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        try
        {
            IdleCheck_UpdateTimestamp();

            // Only the VPN backend clients are granted access
            validate_sender(sender);

            GVariant *retval = nullptr;
            if ("AddIPv4Address" == method_name)
            {
                // Adds a single IPv4 address to the virtual device.  If
                // broadcast has not been provided, calculate it if needed.
            }
            else if ("RemoveIPv4Address" == method_name)
            {
                // Removes a single IPv4 address from the virtual device
            }
            else if ("AddIPv6Address" == method_name)
            {
                // Adds a single IPv6 address to the virtual device
            }
            if ("RemoveIPv6Address" == method_name)
            {
                // Removes a single IPv6 address from the virtual device
            }
            else if ("AddRoutes" == method_name)
            {
                // The caller sends an array of routes to apply
                // It is an array, as this makes everything happen in a
                // single D-Bus method call and it can on some hosts
                // be a considerable amount of routes.  This speeds up
                // the execution
                //
                // The variable signature is not completely decided and
                // must be adopted to what is appropriate
            }
            else if ("RemoveRoutes" == method_name)
            {
                // Similar to AddRoutes, receies an array of routes to
                // remove on this device
            }
            if ("AddDNS" == method_name)
            {
                // Receives a list of DNS server IP addresses to use
                // for DNS lookup
            }
            else if ("RemoveDNS" == method_name)
            {
                // Similar to AddDNS, but receives a list of IP addresses
                // to remove from the DNS lookup
            }
            else if ("AddDNSSearch" == method_name)
            {
                // This is similar to AddDNS, but takes a list of
                // DNS search domains to apply to the system
            }
            if ("RemoveDNSSearch" == method_name)
            {
                // Similar to AddDNSSearch, but removes the search domains
                // provided
            }
            else if ("Activate" == method_name)
            {
                // The virtual device has not yet been created on the host,
                // but all settings which has been queued up will be activated
                // when this method is called.
            }
            else if ("Disable" == method_name)
            {
                // This tears down and disables a virtual device but
                // enables the device to be re-activated again with the same
                // settings by calling the 'Activate' method again
            }
            else if ("Destroy" == method_name)
            {
                // This should run 'Disable' if this has not happened
                // and then this object is completely deleted

                CheckOwnerAccess(sender);
                std::string sender_name = lookup_username(GetUID(sender));
                signal.LogVerb1("Device '" + device_name + "' was removed by "
                               + sender_name);
                RemoveObject(conn);
                g_dbus_method_invocation_return_value(invoc, NULL);
                delete this;
                return;

            }
            g_dbus_method_invocation_return_value(invoc, retval);
            return;
        }
        catch (DBusCredentialsException& excp)
        {
            signal.LogCritical(excp.err());
            excp.SetDBusError(invoc);
        }
        catch (const std::exception& excp)
        {
            std::string errmsg = "Failed executing D-Bus call '" + method_name + "': " + excp.what();
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.netcfg.error.generic",
                                                          errmsg.c_str());
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
        }
        catch (...)
        {
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.netcfg.error.unspecified",
                                                          "Unknown error");
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
        }
    }


    /**
     *   Callback which is used each time a NetCfgServiceObject D-Bus property
     *   is being read.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns a GVariant Glib2 object containing the value of the
     *          requested D-Bus object property.  On errors, NULL must be
     *          returned and the error must be returned via a GError
     *          object.
     */
    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        try
        {
            IdleCheck_UpdateTimestamp();
            validate_sender(sender);

            if ("log_level" == property_name)
            {
                return g_variant_new_uint32(signal.GetLogLevel());
            }
            else if ("owner" == property_name)
            {
                return GetOwner();
            }
            else if ("acl" == property_name)
            {
                return GetAccessList();
            }
            else if ("device_type" == property_name)
            {
                return g_variant_new_uint32((guint) device_type);
            }
            else if ("device_name" == property_name)
            {
                return g_variant_new_string(device_name.c_str());
            }
            else if ("active" == property_name)
            {
                return g_variant_new_boolean(active);
            }
            else if ("ipv4_addresses" == property_name)
            {
                std::vector<std::string> iplist;
                // Popluate iplist, formatted as  "ipaddress/prefix"
                return GLibUtils::GVariantFromVector(iplist);
            }
            else if ("ipv4_routes" == property_name)
            {
                std::vector<std::string> routelist;
                // Popluate routelist, formatted as "ipaddress/prefix=>gw" ?
                return GLibUtils::GVariantFromVector(routelist);
            }
            else if ("ipv6_addresses" == property_name)
            {
                std::vector<std::string> iplist;
                // Popluate iplist, formatted as  "ipaddress/prefix"
                return GLibUtils::GVariantFromVector(iplist);
            }
            else if ("ipv6_routes" == property_name)
            {
                std::vector<std::string> routelist;
                // Popluate routelist, formatted as "ipaddress/prefix=>gw" ?
                return GLibUtils::GVariantFromVector(routelist);
            }
            else if ("dns_servers" == property_name)
            {
                std::vector<std::string> dns_list;
                // Popluate dns_list, formatted as "ipaddress"
                return GLibUtils::GVariantFromVector(dns_list);
            }
            else if ("dns_search" == property_name)
            {
                std::vector<std::string> dns_list;
                // Popluate dns_list, formatted as "search.domain.example.com"
                return GLibUtils::GVariantFromVector(dns_list);
            }
        }
        catch (DBusPropertyException)
        {
            throw;
        }
        catch (DBusException& excp)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
        }
        throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                    obj_path, intf_name, property_name,
                                    "Invalid property");
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property");
        return NULL;
    }


    /**
     *  Callback method which is used each time a NetCfgServiceObject
     *  property is being modified over the D-Bus.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Returns a GVariantBuilder object containing the change
     *         confirmation on success.  On failures, an exception is thrown.
     *
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        try
        {
            IdleCheck_UpdateTimestamp();
            validate_sender(sender);

            if ("log_level" == property_name)
            {
                unsigned int log_level = g_variant_get_uint32(value);
                if (log_level > 6)
                {
                    throw DBusPropertyException(G_IO_ERROR,
                                                G_IO_ERROR_INVALID_DATA,
                                                obj_path, intf_name,
                                                property_name,
                                                "Invalid log level");
                }
                signal.SetLogLevel(log_level);
                return build_set_property_response(property_name,
                                                   (guint32) log_level);
            }
        }
        catch (DBusPropertyException)
        {
            throw;
        }
        catch (DBusException& excp)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
        }
        throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                    obj_path, intf_name, property_name,
                                    "Invalid property");
    }


private:
    std::function<void()> remove_callback;
    NetCfgDeviceType device_type = NetCfgDeviceType::UNSET;
    std::string device_name;
    NetCfgSignals signal;
    bool active = false;

    /**
     *  Validate that the sender is allowed to do change the configuration
     *  for this device.  If not, a DBusCredentialsException is thrown.
     *
     * @param sender  String containing the unique bus ID of the sender
     */
    void validate_sender(std::string sender)
    {
        return;  // FIXME: Currently disabled

        // Only the session manager is susposed to talk to the
        // the backend VPN client service
        if (GetUniqueBusID(OpenVPN3DBus_name_sessions) != sender)
        {
            throw DBusCredentialsException(GetUID(sender),
                                           "net.openvpn.v3.error.acl.denied",
                                           "You are not a session manager"
                                           );
        }
    }
};

