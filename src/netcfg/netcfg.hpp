//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2019  Arne Schwabe <arne@openvpn.net>
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
 * @file   netcfg.cpp
 *
 * @brief  The implementation of the net.openvpn.v3.netcfg D-Bus service
 */

#pragma once

#include <map>

#include <openvpn/common/rc.hpp>

#include "common/lookup.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/glibutils.hpp"
#include "dbus/path.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "dns/settings-manager.hpp"
#include "netcfg-signals.hpp"
#include "netcfg-subscriptions.hpp"
#include "netcfg-device.hpp"
#include "netcfg-options.hpp"

using namespace openvpn;
using namespace NetCfg;

/**
 *  Main D-Bus entry-point object for the net.openvpn.v3.netcfg service
 *
 */
class NetCfgServiceObject : public DBusObject,
                            public DBusConnectionCreds,
                            public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<NetCfgServiceObject> Ptr;

    /**
     *  Initialize the main Network Configuration service object.  This
     *  is the entrypoint object used by the backend VPN client process
     *
     * @param conn               D-Bus connection to use
     * @param default_log_level  Default log level to start with
     * @param logwr              LogWriter object which tackles local logging
     */
    NetCfgServiceObject(GDBusConnection *conn,
                        const unsigned int default_log_level,
                        DNS::SettingsManager::Ptr resolver,
                        LogWriter *logwr,
                        NetCfgOptions options)
        : DBusObject(OpenVPN3DBus_rootp_netcfg),
          DBusConnectionCreds(conn),
          signal(conn, LogGroup::NETCFG, OpenVPN3DBus_rootp_netcfg, logwr),
          resolver(resolver),
          creds(conn),
          options(std::move(options))
    {
        signal.SetLogLevel(default_log_level);

        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << OpenVPN3DBus_rootp_netcfg << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_netcfg << "'>"
                          << "        <method name='CreateVirtualInterface'>"
                          << "          <arg type='s' direction='in' name='device_name'/>"
                          << "          <arg type='o' direction='out' name='device_path'/>"
                          << "        </method>"
                          << "        <method name='FetchInterfaceList'>"
                          << "          <arg type='ao' direction='out' name='device_paths'/>"
                          << "        </method>"
                          << "        <method name='ProtectSocket'>"
                          << "          <arg type='s' direction='in' name='remote' />"
                          << "          <arg type='b' direction='in' name='ipv6' />"
                          << "          <arg type='o' direction='in' name='device_path' />"
                          << "          <arg type='b' direction='out' name='succeded'/>"
                          << "        </method>"
                          << "        <method name='DcoAvailable'>"
                          << "          <arg type='b' direction='out' name='available'/>"
                          << "        </method>"
                          << "        <method name='Cleanup'>"
                          << "        </method>"
                          << NetCfgSubscriptions::GenIntrospection("NotificationSubscribe",
                                                                   "NotificationUnsubscribe",
                                                                   "NotificationSubscriberList")
                          /* The fd that this method gets is not in the function signature */
                          << "    <property type='u' name='global_dns_servers' access='read'/>"
                          << "    <property type='u' name='global_dns_search' access='read'/>"
                          << "    <property type='u' name='log_level' access='readwrite'/>"
                          << "    <property type='s' name='config_file' access='read'/>"
                          << "    <property type='s' name='version' access='read'/>"
                          << signal.GetLogIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);
        signal.Debug("Network Configuration service object ready");
        if (!resolver)
        {
            signal.LogWarn("No DNS resolver has been configured");
        }
    }


    ~NetCfgServiceObject() override
    {
    }


    /**
     *  Enables the subscription list management for NetworkChange signals
     *
     * @param subs  NetCfgSubscription::Ptr to an instantiated subscriptions
     *              management object
     */
    void ConfigureSubscriptionManager(NetCfgSubscriptions::Ptr subs)
    {
        subscriptions = subs;
    }


    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this NetCfgServiceObject.
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
                              GDBusMethodInvocation *invoc) override
    {
        try
        {
            IdleCheck_UpdateTimestamp();

            // Only the VPN backend clients are granted access
            validate_sender(sender);

            GVariant *retval = nullptr;
            if ("CreateVirtualInterface" == method_name)
            {
                gchar *dev_name = nullptr;
                g_variant_get(params, "(s)", &dev_name);
                std::string device_name(dev_name);
                g_free(dev_name);

                signal.Debug(std::string("CreateVirtualInterface(")
                             + "'" + device_name + "')");
                std::string dev_path = createVirtualDevice(conn, sender, device_name);
                retval = g_variant_new("(o)", dev_path.c_str());

            }
            else if ("FetchInterfaceList" == method_name)
            {
                // Build up an array of object paths to available devices
                GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("ao"));

                for (const auto& dev : devices)
                {
                    g_variant_builder_add(bld, "o", dev.first.c_str());
                }

                // Wrap up the result into a tuple, which GDBus expects and
                // put it into the invocation response
                GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(ret, g_variant_builder_end(bld));
                retval = g_variant_builder_end(ret);

                // Clean-up
                g_variant_builder_unref(bld);
                g_variant_builder_unref(ret);
            }
            else if ("ProtectSocket" == method_name)
            {
                retval = protect_socket(GetPID(sender), conn, invoc, params);
            }
            else if ("DcoAvailable" == method_name)
            {
                bool available = false;
#ifdef ENABLE_OVPNDCO
                available = NetCfgDCO::available();
#endif
                retval = g_variant_new("(b)", available);
            }
            else if ("Cleanup" == method_name)
            {
                cleanup_process_resources(GetPID(sender), conn);
                retval = g_variant_new("()");
            }
            else if ("NotificationSubscribe" == method_name)
            {
                if (!subscriptions)
                {
                    signal.LogWarn("Ignored NotificationSubscribe request "
                                    "from " + sender + ", "
                                    "subscription management disabled");
                    throw NetCfgException("Notification subscription disabled");
                }

                //  By default, the subscribe method access is managed by
                //  the D-Bus policy.  The default policy will only allow
                //  this by the openvpn user account.

                // Use a larger data type to allow better input validation
                uint32_t filter_flags = 0;
                g_variant_get(params, "(u)", &filter_flags);
                subscriptions->Subscribe(sender, filter_flags);

                std::stringstream msg;
                msg << "New subscription: '" << sender << "' => "
                    << NetCfgChangeEvent::FilterMaskStr(filter_flags, true);
                signal.LogVerb2(msg.str());
                IdleCheck_RefInc();
            }
            else if ("NotificationUnsubscribe" == method_name)
            {
                if (!subscriptions)
                {
                    signal.LogWarn("Ignored NotificationUnsubscribe request "
                                    "from " + sender + ", "
                                    "subscription management disabled");
                    throw NetCfgException("Notification subscription disabled");
                }

                //  By default, an external utility can only unsubscribe its
                //  own subscription while the root user accounts can
                //  unsubscribe any subscriber.
                //
                //  Who can call the unsubscribe methods is also managed by
                //  the D-Bus policy.  The default policy will only allow
                //  this by the openvpn and root user accounts.

                // Only retrieve the subscriber value if the call comes
                // from an admin user
                std::string sub(sender);
                uid_t uid = GetUID(sender);
                if (0 == uid )
                {
                    gchar *sub_c = nullptr;
                    g_variant_get(params, "(s)", &sub_c);
                    if (nullptr == sub_c)
                    {
                        signal.LogCritical("Failed to retrieve subscriber");
                        throw NetCfgException("Failed to retrieve subscriber");
                    }
                    if (strlen(sub_c) > 0)
                    {
                        sub = std::string(sub_c);
                    }
                    g_free(sub_c);
                }

                subscriptions->Unsubscribe(sub);
                signal.LogVerb2("Unsubscribed subscription: '"+ sub + "'");
                IdleCheck_RefDec();
            }
            else if ("NotificationSubscriberList" == method_name)
            {
                if (!subscriptions)
                {
                    signal.LogWarn("Ignored NotificationSubscriberList "
                                    "request from " + sender + ", "
                                    "subscription management disabled");
                    throw NetCfgException("Notification subscription disabled");
                }

                // Only allow this method to be accessible by root
                uid_t uid = GetUID(sender);
                if (0 != uid )
                {
                    throw DBusCredentialsException(uid,
                                                   "net.openvpn.v3.error.acl.denied",
                                                   "Access denied");
                }

                g_dbus_method_invocation_return_value(invoc,
                                                      subscriptions->List());
                return;
            }
            else
            {
                throw std::invalid_argument("Not implemented method");
            }

            g_dbus_method_invocation_return_value(invoc, retval);
            return;
        }
        catch (DBusCredentialsException& excp)
        {
            signal.LogCritical(excp.what());
            excp.SetDBusError(invoc);
        }
        catch (const NetCfgDeviceException& excp)
        {
            signal.LogCritical(excp.what());
            excp.SetDBusError(invoc, "net.openvpn.v3.netcfg.error");
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

    std::string createVirtualDevice(GDBusConnection *conn, const std::string& sender, const std::string& dev_name)
    {
        std::string dev_path = OpenVPN3DBus_rootp_netcfg + "/"
                               + std::to_string(creds.GetPID(sender))
                               + "_" + dev_name;
        NetCfgDevice *device = new NetCfgDevice(conn,
                                                [self = Ptr(this), dev_path]()
                                                {
                                                    self->remove_device_object(dev_path);
                                                },
                                                creds.GetUID(sender),
                                                creds.GetPID(sender),
                                                dev_path,
                                                dev_name, resolver, subscriptions.get(),
                                                signal.GetLogLevel(),
                                                signal.GetLogWriter(),
                                           options);


        IdleCheck_RefInc();
        device->IdleCheck_Register(IdleCheck_Get());
        device->RegisterObject(conn);
        devices[dev_path] = device;

        signal.LogInfo(std::string("Virtual device '") + dev_name + "'"
                       + " registered on " + dev_path
                       + " (owner uid " + std::to_string(creds.GetUID(sender))
                       + ", owner pid "+ std::to_string(creds.GetPID(sender)) + ")");

        return dev_path;
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
                                     GError **error) override
    {
        try
        {
            IdleCheck_UpdateTimestamp();
            if ("log_level" == property_name)
            {
                return g_variant_new_uint32(signal.GetLogLevel());
            }
            else if ("config_file" == property_name)
            {
                return g_variant_new_string(options.config_file.c_str());
            }
            else if ("global_dns_servers" == property_name)
            {
                if (!resolver)
                {
                    // If no resolver is configured, return an empty result
                    // instead of an error when reading this property
                    return GLibUtils::GVariantFromVector(std::vector<std::string>{});
                }
                return GLibUtils::GVariantFromVector(resolver->GetDNSservers());
            }
            else if ("global_dns_search" == property_name)
            {
                if (!resolver)
                {
                    // If no resolver is configured, return an empty result
                    // instead of an error when reading this property
                    return GLibUtils::GVariantFromVector(std::vector<std::string>{});
                }
                return GLibUtils::GVariantFromVector(resolver->GetSearchDomains());
            }
            else if ("version" == property_name)
            {
                return g_variant_new_string(package_version());
            }
        }
        catch (...)
        {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Unknown error");
        }
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
                                            GError **error) override
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
        catch (DBusPropertyException&)
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
    NetCfgSignals signal;
    DNS::SettingsManager::Ptr resolver;
    DBusConnectionCreds creds;
    std::map<std::string, NetCfgDevice *> devices;
    NetCfgOptions options;
    NetCfgSubscriptions::Ptr subscriptions;


    /**
     *  Validate that the sender is allowed to do network configuration.
     *  If not, a DBusCredentialsException is thrown.
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


    /**
     * Clean up function that removes all objects that are still around from
     * a process like registred virtual devices and socket protections
     */
    void cleanup_process_resources(pid_t pid, GDBusConnection* conn)
    {
        signal.LogInfo(std::string("Cleaning up resources for PID ")
                           + std::to_string(pid) + ".");

        // Just normal loop here, since we delete from the container while modifying it
        for (auto it = devices.cbegin(); it != devices.cend(); )
        {
            auto tundev = it->second;
            if (tundev->getCreatorPID() == pid)
            {
                // The teardown method will also call to the erase method which will
                // then be a noop but doing the erase here gets us a valid next iterator
                it = devices.erase(it);
                tundev->teardown(conn);
                delete tundev;
            }
            else
            {
                // normal loop iterate
                it++;
            }
        }
        openvpn::cleanup_protected_sockets(pid);
    }

    /**
     * Callback function used by NetCfgDevice instances to remove
     * its object path from the main registry of device objects
     *
     * @param devgpath  std::string containing the object path of the object
     *                  to remove
     */
    void remove_device_object(const std::string devpath)
    {
        devices.erase(devpath);
    }


    /**
     * Reads a unix fd from a connec and protects that socket from being
     * routed over the VPN
     *
     * @param conn   GDBusConnection pointer where the request came
     * @param invoc  GDBusMethodInvocation pointer containing the request.
     *               The file descriptor to be protected must come in the
     *               message inside this request.
     *
     * @return  Returns a GVariant object with the reply to the caller.
     *          This will always be a boolean true value on success.  In case
     *          of errors, a NetCfgException is thrown.
     */
    GVariant* protect_socket(pid_t creator_pid, GDBusConnection *conn,
                             GDBusMethodInvocation *invoc,
                             GVariant *params)
    {
        // This should generally be true for DBus 1.3, double checking here cannot hurt
        g_assert(g_dbus_connection_get_capabilities(conn) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

        GLibUtils::checkParams(__func__, params, "(sbo)", 3);

        std::string remote(g_variant_get_string(g_variant_get_child_value(params, 0), 0));
        bool ipv6 = g_variant_get_boolean(g_variant_get_child_value(params, 1));
        std::string dev_path(g_variant_get_string(g_variant_get_child_value(params, 2), 0));

        // Get the first FD from the fdlist list
        int fd = GLibUtils::get_fd_from_invocation(invoc);

        // If the devpath is valid we get the device name from it to ignore it in the host route to avoid
        // routing loops

        std::string tunif;
        const auto& it = devices.find(dev_path);
        if (it != devices.end())
        {
            const auto& dev = *it->second;
            tunif = dev.get_device_name();
        }

        signal.LogInfo(std::string("Socket protect called for socket ")
                           + std::to_string(fd)
                           + ", remote: '" + remote
                           + "', tun: '" + tunif
                           + "', ipv6: " + (ipv6 ? "yes" : "no"));

        if (options.so_mark >= 0)
        {
            if (fd == -1)
            {
                throw NetCfgException("SO_MARK requested but protect_socket call received no fd");
            }
            openvpn::protect_socket_somark(fd, remote, options.so_mark);
        }
        if (RedirectMethod::BINDTODEV == options.redirect_method)
        {
            if (fd == -1)
            {
                throw NetCfgException("bind to dev method requested but protect_socket call received no fd");
            }
            openvpn::protect_socket_binddev(fd, remote, ipv6);
        }
        if (options.redirect_method == RedirectMethod::HOST_ROUTE)
        {
            openvpn::cleanup_protected_sockets(creator_pid);
            openvpn::protect_socket_hostroute(tunif, remote, ipv6, creator_pid);
        }
        close(fd);
        return g_variant_new("(b)", true);
    }
};


class NetworkCfgService : public DBus
{
public:
    /**
     *  Initializes the NetworkCfgService object
     *
     * @param bus_type   GBusType, which defines if this service should be
     *                   registered on the system or session bus.
     * @param logwr      LogWriter object which takes care of the log processing
     *
     */
    NetworkCfgService(GDBusConnection *dbuscon,
                      DNS::SettingsManager::Ptr resolver,
                      LogWriter *logwr,
                      NetCfgOptions options)
        : DBus(dbuscon,
               OpenVPN3DBus_name_netcfg,
               OpenVPN3DBus_rootp_netcfg,
               OpenVPN3DBus_interf_netcfg),
          resolver(resolver),
          logwr(logwr),
          default_log_level(4),
          signal(nullptr),
          srv_obj(nullptr),
          options(std::move(options))
    {
    }


    ~NetworkCfgService() = default;


    /**
     *  Sets the default log level when the backend client starts.  This
     *  can later on be adjusted by modifying the log_level D-Bus object
     *  property.  When not being changed, the default log level is 6.
     *
     * @param lvl  Unsigned integer of the default log level.
     */
    void SetDefaultLogLevel(unsigned int lvl)
    {
        default_log_level = lvl;
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        // Setup a signal object of the backend
        signal.reset(new NetCfgSignals(GetConnection(), LogGroup::NETCFG,
                                       OpenVPN3DBus_rootp_netcfg, logwr));
        signal->SetLogLevel(default_log_level);

        // Create a new OpenVPN3 client session object
        srv_obj.reset(new NetCfgServiceObject(GetConnection(),
                                              default_log_level,
                                              resolver,
                                              logwr, options
                                              ));
        srv_obj->RegisterObject(GetConnection());
        if (!options.signal_broadcast)
        {
            subscriptions.reset(new NetCfgSubscriptions);
            srv_obj->ConfigureSubscriptionManager(subscriptions);
        }
        else
        {
            signal->LogCritical("Signal broadcast enabled, "
                                "which includes NetworkChange signals");
        }

        signal->Debug("NetCfg service registered on '" + GetBusName()
                       + "': " + OpenVPN3DBus_rootp_netcfg);

        // Log which redirect method is in use
        signal->LogVerb1(options.str());

        if (resolver)
        {
            signal->LogVerb2(resolver->GetBackendInfo());

            // Do a ApplySettings() call now, which will only fetch the
            // current system settings and initialize the resolver backend
            resolver->ApplySettings(signal.get());
        }

        if (nullptr != idle_checker)
        {
            srv_obj->IdleCheck_Register(idle_checker);
        }
    }


    /**
     *  This is called each time the well-known bus name is successfully
     *  acquired on the D-Bus.
     *
     *  This is not used, as the preparations already happens in
     *  callback_bus_acquired()
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the acquired bus name
     */
    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    };


    /**
     *  This is called each time the well-known bus name is removed from the
     *  D-Bus.  In our case, we just throw an exception and starts shutting
     *  down.
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the lost bus name
     */
    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        std::stringstream msg;
        msg << "Lost the D-Bus connection and the '" + busname + "' "
            << "bus name. Most likely related to an internal "
            << "issue in dbus-daemon. Check ";
#ifdef ENABLE_SELINUX_BUILD
        msg << "if SELinux dbus_access_tuntap_device boolean is enabled, "
            << "otherwise ";
#endif
        msg << "available audit logs.";
        THROW_DBUSEXCEPTION("openvpn-service-netcfg", msg.str());
    };


private:
    DNS::SettingsManager::Ptr resolver;
    LogWriter *logwr;

    unsigned int default_log_level;
    NetCfgSignals::Ptr signal;
    NetCfgSubscriptions::Ptr subscriptions;
    NetCfgServiceObject::Ptr srv_obj;
    NetCfgOptions options;
};
