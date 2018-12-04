//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2018  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2018  David Sommerseth <davids@openvpn.net>
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

#ifndef OPENVPN3_DBUS_CONFIGMGR_HPP
#define OPENVPN3_DBUS_CONFIGMGR_HPP

#include <functional>
#include <map>
#include <ctime>

#include <openvpn/log/logsimple.hpp>
#include "common/core-extensions.hpp"
#include "common/lookup.hpp"
#include "common/utils.hpp"
#include "configmgr/overrides.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/exceptions.hpp"
#include "dbus/object-property.hpp"
#include "log/ansicolours.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"

using namespace openvpn;


/**
 * Helper class to tackle signals sent by the configuration manager
 *
 * This mostly just wraps the LogSender class and predefines LogGroup to always
 * be CONFIGMGR.
 */

class ConfigManagerSignals : public LogSender
{
public:
    /**
     *  Declares the ConfigManagerSignals object
     *
     * @param conn        D-Bus connection to use when sending signals
     * @param object_path D-Bus object to use as sender of signals
     * @param default_log_level  Unsigned integer defining the initial log level
     * @param signal_broadcast Should signals be broadcasted (true) or
     *                         targeted for the log service (false)
     * @param logwr       Pointer to a LogWriter object for file logging.
     *                    Can be nullptr, which results in no active file logging.
     */
    ConfigManagerSignals(GDBusConnection *conn, std::string object_path,
                         unsigned int default_log_level, LogWriter *logwr,
                         bool signal_broadcast = true)
        : LogSender(conn, LogGroup::CONFIGMGR,
                    OpenVPN3DBus_interf_configuration, object_path,
                    logwr),
          logwr(logwr),
          signal_broadcast(signal_broadcast)
    {
        SetLogLevel(default_log_level);
        if (!signal_broadcast)
        {
            DBusConnectionCreds credsprx(conn);
            AddTargetBusName(credsprx.GetUniqueBusID(OpenVPN3DBus_name_log));
        }
    }


    virtual ~ConfigManagerSignals()
    {
    }


    LogWriter *GetLogWriterPtr()
    {
        return logwr;
    }


    bool GetSignalBroadcast()
    {
        return signal_broadcast;
    }

    /**
     *  Whenever a FATAL error happens, the process is expected to stop.
     *  (The exit step is not yet implemented)
     *
     * @param msg  Message to sent to the log subscribers
     */
    void LogFATAL(std::string msg)
    {
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        // FIXME: throw something here, to start shutdown procedures
    }


private:
    LogWriter *logwr = nullptr;
    bool signal_broadcast = true;
};


/**
 *  ConfigurationAlias objects are present under the
 *  /net/openvpn/v3/configuration/alias/$ALIAS_NAME D-Bus path in the
 *  configuration manager service.  They are simpler aliases which can be
 *  used instead of the full D-Bus object path to a single VPN configuration
 *  profile
 */
class ConfigurationAlias : public DBusObject,
                           public ConfigManagerSignals
{
public:
    /**
     * Initializes a Configuration Alias
     *
     * @param dbuscon    D-Bus connection to use for this object
     * @param aliasname  A string containing the alias name
     * @param cfgpath    An object path pointing at an existing D-Bus path
     * @param default_log_level  Unsigned integer defining the initial log level
     * @param logwr      Pointer to LogWriter object; can be nullptr to disable
     *                   file log.
     * @param signal_broadcast Should signals be broadcasted (true) or
     *                         targeted for the log service (false)
     */
    ConfigurationAlias(GDBusConnection *dbuscon,
                       std::string aliasname, std::string cfgpath,
                       unsigned int default_log_level, LogWriter *logwr,
                       bool signal_broadcast)
        : DBusObject(OpenVPN3DBus_rootp_configuration + "/aliases/" + aliasname),
          ConfigManagerSignals(dbuscon, OpenVPN3DBus_rootp_configuration + "/aliases/" + aliasname,
                               default_log_level, logwr, signal_broadcast),
          cfgpath(cfgpath)
    {
        alias = aliasname;
        std::string new_obj_path = OpenVPN3DBus_rootp_configuration + "/aliases/"
                                   + aliasname;

        if (!g_variant_is_object_path(new_obj_path.c_str()))
        {
            THROW_DBUSEXCEPTION("ConfigurationAlias",
                                "Specified alias is invalid");
        }

        std::string introsp_xml ="<node name='" + new_obj_path + "'>"
            "    <interface name='" + OpenVPN3DBus_interf_configuration + "'>"
            "        <property  type='o' name='config_path' access='read'/>"
            "    </interface>"
            "</node>";
        ParseIntrospectionXML(introsp_xml);
    }


    /**
     *  Returns a C string (char *) of the configured alias name

     * @return Returns a const char * pointing at the alias name
     */
    const char * GetAlias()
    {
        return alias.c_str();
    }


    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this ConfigurationAlias object.
     *
     *  This object does not have any methods, so it is not expected that
     *  this callback will be called.  If it is, throw an exception.
     *
     * @param conn       D-Bus connection where the method call occurred
     * @param sender     D-Bus bus name of the sender of the method call
     * @param obj_path   D-Bus object path of the target object.
     * @param intf_name  D-Bus interface of the method call
     * @param method_name D-Bus method name to be executed
     * @param params     GVariant Glib2 object containing the arguments for
     *                   the method call
     * @param invoc      GDBusMethodInvocation where the response/result of
     *                   the method call will be returned.
     */
    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        THROW_DBUSEXCEPTION("ConfigManagerAlias", "method_call not implemented");
    }


    /**
     *   Callback which is called each time a ConfigurationAlias D-Bus
     *   property is being read.
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
        GVariant *ret = NULL;

        if ("config_path" == property_name)
        {
            ret = g_variant_new_string (cfgpath.c_str());
        }
        else
        {
            ret = NULL;
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "Unknown property");
        }
        return ret;
    };

    /**
     *  Callback method which is used each time a ConfigurationAlias
     *  property is being modified over the D-Bus.
     *
     *  This will always fail with an exception, as there exists no properties
     *  which can be modified in a ConfigurationAlias object.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Will always throw an exception as there are no properties to
     *         modify.
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("ConfigManagerAlias", "set property not implemented");
    }


private:
    std::string alias;
    std::string cfgpath;
};


/**
 *  Specialised template for managing the override list property
 */
template <>
class PropertyType<std::vector<OverrideValue>> : public PropertyTypeBase<std::vector<OverrideValue>>
{
public:
    PropertyType<std::vector<OverrideValue>>(DBusObject *obj_arg,
                                          std::string name_arg,
                                          std::string dbus_acl_arg,
                                          bool allow_mngr_arg,
                                          std::vector<OverrideValue>& value_arg)
        : PropertyTypeBase<std::vector<OverrideValue>>(obj_arg,
                                                     name_arg,
                                                     dbus_acl_arg,
                                                     allow_mngr_arg,
                                                     value_arg)
    {
    }

    virtual GVariant *GetValue() const override
    {
        GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        for (const auto& e : this->value)
        {
            GVariant* value;
            if (OverrideType::string == e.override.type)
            {
                value = g_variant_new("s", e.strValue.c_str());
            }
            else
            {
                value = g_variant_new("b", e.boolValue);
            }
            g_variant_builder_add(bld, "{sv}", e.override.key.c_str(), value);
        }
        GVariant* ret = g_variant_builder_end(bld);
        g_variant_builder_unref(bld);
        return ret;
    }

    virtual const char* GetDBusType() const override
    {
        return "a{sv}";
    }

    // This property is readonly and need to be modified with Add/UnsetOverride
    virtual GVariantBuilder *SetValue(GVariant *value_arg) override
    {
        return nullptr;
    }
};


/**
 *  A ConfigurationObject contains information about a specific VPN
 *  configuration profile.  This object is then exposed on the D-Bus through
 *  its own unique object path.
 *
 *  The configuration manager is responsible for maintaining the
 *  life cycle of these configuration objects.
 */
class ConfigurationObject : public DBusObject,
                            public ConfigManagerSignals,
                            public DBusCredentials
{
public:
    /**
     *  Constructor creating a new ConfigurationObject
     *
     * @param dbuscon  D-Bus connection this object is tied to
     * @param remove_callback  Callback function which must be called when
     *                 destroying this configuration object.
     * @param objpath  D-Bus object path of this object
     * @param default_log_level  Unsigned integer defining the initial log level
     * @param logwr    Pointer to LogWriter object; can be nullptr to disable
     *                 file log.
     * @param signal_broadcast Should signals be broadcasted (true) or
     *                         targeted for the log service (false)
     * @param creator  An uid reference of the owner of this object.  This is
     *                 typically the uid of the front-end user importing this
     *                 VPN configuration profile.
     * @param params   Pointer to a GLib2 GVariant object containing both
     *                 meta data as well as the configuration profile itself
     *                 to use when initializing this object
     */
    ConfigurationObject(GDBusConnection *dbuscon,
                        std::function<void()> remove_callback,
                        std::string objpath, unsigned int default_log_level,
                        LogWriter *logwr, bool signal_broadcast,
                        uid_t creator, GVariant *params)
        : DBusObject(objpath),
          ConfigManagerSignals(dbuscon, objpath, default_log_level, logwr,
                               signal_broadcast),
          DBusCredentials(dbuscon, creator),
          remove_callback(remove_callback),
          name(""),
          import_tstamp(std::time(nullptr)),
          last_use_tstamp(0),
          used_count(0),
          valid(false),
          readonly(false),
          single_use(false),
          persistent(false),
          alias(nullptr),
          properties(this)
    {
        gchar *cfgstr = nullptr;
        gchar *cfgname_c = nullptr;
        g_variant_get (params, "(ssbb)",
                       &cfgname_c, &cfgstr,
                       &single_use, &persistent);
        name = std::string(cfgname_c);

        // Parse the options from the imported configuration
        OptionList::Limits limits("profile is too large",
				  ProfileParseLimits::MAX_PROFILE_SIZE,
				  ProfileParseLimits::OPT_OVERHEAD,
				  ProfileParseLimits::TERM_OVERHEAD,
				  ProfileParseLimits::MAX_LINE_SIZE,
				  ProfileParseLimits::MAX_DIRECTIVE_SIZE);
        options.parse_from_config(cfgstr, &limits);

        std::stringstream msg;
        msg << "Parsed "
            << (persistent ? "persistent" : "")
            << (persistent && single_use ? ", " : "")
            << (single_use ? "single-use" : "")
            << " configuration '" << name << "'"
            << ", owner: " << lookup_username(creator);
        LogInfo(msg.str());

        // FIXME:  Validate the configuration file, ensure --ca/--key/--cert/--dh/--pkcs12
        //         contains files
        valid = true;

        properties.AddBinding(new PropertyType<std::time_t>(this, "import_timestamp", "read", false, import_tstamp, "t"));
        properties.AddBinding(new PropertyType<std::time_t>(this, "last_used_timestamp", "read", false, last_use_tstamp, "t"));
        properties.AddBinding(new PropertyType<bool>(this, "locked_down", "readwrite", false, locked_down));
        properties.AddBinding(new PropertyType<bool>(this, "persistent", "read", false, persistent));
        properties.AddBinding(new PropertyType<bool>(this, "readonly", "read", false, readonly));
        properties.AddBinding(new PropertyType<bool>(this, "single_use", "read", false, single_use));
        properties.AddBinding(new PropertyType<unsigned int>(this, "used_count", "read", false, used_count));
        properties.AddBinding(new PropertyType<bool>(this, "valid", "read", false, valid));
        properties.AddBinding(new PropertyType<decltype(override_list)>(this, "overrides", "read", true, override_list));

        std::string introsp_xml ="<node name='" + objpath + "'>"
            "    <interface name='net.openvpn.v3.configuration'>"
            "        <method name='Fetch'>"
            "            <arg direction='out' type='s' name='config'/>"
            "        </method>"
            "        <method name='FetchJSON'>"
            "            <arg direction='out' type='s' name='config_json'/>"
            "        </method>"
            "        <method name='SetOption'>"
            "            <arg direction='in' type='s' name='option'/>"
            "            <arg direction='in' type='s' name='value'/>"
            "        </method>"
            "        <method name='SetOverride'>"
            "            <arg direction='in' type='s' name='name'/>"
            "            <arg direction='in' type='v' name='value'/>"
            "        </method>"
            "        <method name='UnsetOverride'>"
            "            <arg direction='in' type='s' name='name'/>"
            "        </method>"
            "        <method name='AccessGrant'>"
            "            <arg direction='in' type='u' name='uid'/>"
            "        </method>"
            "        <method name='AccessRevoke'>"
            "            <arg direction='in' type='u' name='uid'/>"
            "        </method>"
            "        <method name='Seal'/>"
            "        <method name='Remove'/>"
            "        <property type='u' name='owner' access='read'/>"
            "        <property type='au' name='acl' access='read'/>"
            "        <property type='s' name='name' access='readwrite'/>"
            "        <property type='b' name='public_access' access='readwrite'/>"
            "        <property type='s' name='alias' access='readwrite'/>"
            + properties.GetIntrospectionXML() +
            "    </interface>"
            "</node>";
        ParseIntrospectionXML(introsp_xml);

        g_free(cfgname_c);
        g_free(cfgstr);
    }


    ~ConfigurationObject()
    {
        remove_callback();
        Debug("Configuration removed");
        IdleCheck_RefDec();
    };


    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this ConfigurationObject.
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
        IdleCheck_UpdateTimestamp();
        if ("Fetch" == method_name)
        {
            try
            {
                if (!locked_down)
                {
                    CheckACL(sender, true);
                }
                else
                {
                    // If the configuration is locked down, restrict any
                    // read-operations to anyone except the backend VPN client
                    // process (openvpn user) or the configuration profile
                    // owner
                    CheckOwnerAccess(sender, true);
                }
                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(s)",
                                                                    options.string_export().c_str()));

                // If the fetching user is openvpn (which
                // openvpn3-service-client runs as), we consider this
                // configuration to be "used"
                if (GetUID(sender) == lookup_uid(OPENVPN_USERNAME))
                {
                    // If this config is tagged as single-use only then we delete this
                    // config from memory.
                    if (single_use)
                    {
                        LogVerb2("Single-use configuration fetched");
                        RemoveObject(conn);
                        delete this;
                        return;
                    }
                    used_count++;
                    last_use_tstamp = std::time(nullptr);
                }
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("FetchJSON" == method_name)
        {
            try
            {
                if (!locked_down)
                {
                    CheckACL(sender);
                }
                else
                {
                    // If the configuration is locked down, restrict any
                    // read-operations to the configuration profile owner
                    CheckOwnerAccess(sender);
                }
                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(s)",
                                                                    options.json_export().c_str()));

                // Do not remove single-use object with this method.
                // FetchJSON is only used by front-ends, never backends.  So
                // it still needs to be available when the backend calls Fetch.
                //
                // single-use configurations are an automation convenience,
                // not a security feature.  Security is handled via ACLs.
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("SetOption" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }
            try
            {
                CheckOwnerAccess(sender);
                // TODO: Implement SetOption
                g_dbus_method_invocation_return_value(invoc, NULL);
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("SetOverride" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error(invoc,
                                                           "net.openvpn.v3.error.ReadOnly",
                                                           "Configuration is sealed and readonly");
                return;
            }
            try
            {
                CheckOwnerAccess(sender);
                gchar *key = nullptr;
                GVariant *val = nullptr;
                g_variant_get(params, "(sv)", &key, &val);

                const OverrideValue vo = set_override(key, val);

                std::string newValue = vo.strValue;
                if (OverrideType::boolean == vo.override.type)
                {
                    newValue = vo.boolValue ? "true" : "false";
                }

                LogInfo("Setting configuration override '" + std::string(key)
                            + "' to '" + newValue + "' by UID " + std::to_string(GetUID(sender)));

                g_free(key);
                //g_variant_unref(val);
                g_dbus_method_invocation_return_value(invoc, NULL);
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
            catch (DBusException& excp)
            {
                LogWarn(excp.what());
                excp.SetDBusError(invoc, "net.openvpn.v3.configmgr.error");
            }
        }
        else if ("UnsetOverride" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error(invoc,
                                                           "net.openvpn.v3.error.ReadOnly",
                                                           "Configuration is sealed and readonly");
                return;
            }
            try
            {
                CheckOwnerAccess(sender);
                gchar *key = nullptr;
                g_variant_get(params, "(s)", &key);
                if(remove_override(key))
                {
                    LogInfo("Unset configuration override '" + std::string(key)
                                + "' by UID " + std::to_string(GetUID(sender)));

                    g_dbus_method_invocation_return_value(invoc, NULL);
                }
                else
                {
                    std::stringstream err;
                    err << "Override '" << std::string(key) << "' has "
                        << "not been set";
                    g_dbus_method_invocation_return_dbus_error (invoc,
                                                                "net.openvpn.v3.error.OverrideNotSet",
                                                                err.str().c_str());
                }
                g_free(key);
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("AccessGrant" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }

            try
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                GrantAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogInfo("Access granted to UID " + std::to_string(uid)
                         + " by UID " + std::to_string(GetUID(sender)));
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("AccessRevoke" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }

            try
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                RevokeAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogInfo("Access revoked for UID " + std::to_string(uid)
                         + " by UID " + std::to_string(GetUID(sender)));
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("Seal" == method_name)
        {
            try
            {
                CheckOwnerAccess(sender);

                if (valid) {
                    readonly = true;
                    g_dbus_method_invocation_return_value(invoc, NULL);
                }
                else
                {
                    g_dbus_method_invocation_return_dbus_error (invoc,
                                                                "net.openvpn.v3.error.InvalidData",
                                                                "Configuration is not currently valid");
                }
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("Remove" == method_name)
        {
            try
            {
                CheckOwnerAccess(sender);
                std::string sender_name = lookup_username(GetUID(sender));
                LogInfo("Configuration '" + name + "' was removed by "
                        + sender_name);
                RemoveObject(conn);
                g_dbus_method_invocation_return_value(invoc, NULL);
                delete this;
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
    };


    /**
     *   Callback which is used each time a ConfigurationObject D-Bus
     *   property is being read.
     *
     *   Only the 'owner' is accessible by anyone, otherwise it must either
     *   be the configuration owner or UIDs granted access to this profile.
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
        IdleCheck_UpdateTimestamp();

        // Properties available for everyone
        if ("owner" == property_name)
        {
            return GetOwner();
        }

        // Properties available for the openvpn user (OpenVPN Manager)
        bool allow_mngr = false;
        if (properties.Exists(property_name))
        {
            allow_mngr = properties.GetManagerAllowed(property_name);
        }
        else if ("name" == property_name)
        {
            // Grant the openvpn user access to the 'name' property
            allow_mngr = true;
        }


        // Properties only available for approved users
        try {
            CheckACL(sender, allow_mngr);

            GVariant *ret = NULL;

            if ("name"  == property_name)
            {
                ret = g_variant_new_string (name.c_str());
            }
            else if ("alias" == property_name)
            {
                ret = g_variant_new_string(alias ? alias->GetAlias() : "");
            }
            else if ("public_access" == property_name)
            {
                ret = GetPublicAccess();
            }
            else if ("acl" == property_name)
            {
                    ret = GetAccessList();
            }
            else if (properties.Exists(property_name))
            {
                ret = properties.GetValue(property_name);
            }
            else
            {
                g_set_error (error,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Unknown property");
            }

            return ret;
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_FAILED);
            return NULL;
        }
    };


    /**
     *  Callback method which is used each time a ConfigurationObject property
     *  is being modified over D-Bus.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Returns a GVariantBuilder object which will be used by the
     *         D-Bus library to issue the required PropertiesChanged signal.
     *         If an error occurres, the DBusPropertyException is thrown.
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        IdleCheck_UpdateTimestamp();
        if (readonly)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_READ_ONLY,
                                        obj_path, intf_name, property_name,
                                        "Configuration object is read-only");
        }

        try
        {
            CheckOwnerAccess(sender);

            GVariantBuilder * ret = NULL;
            if (("alias" == property_name) && conn)
            {
                if (nullptr != alias)
                {
                    alias->RemoveObject(conn);
                    delete alias;
                    alias = nullptr;
                }

                try
                {
                    gsize len = 0;
                    alias =  new ConfigurationAlias(conn,
                                                    std::string(g_variant_get_string(value, &len)),
                                                    GetObjectPath(),
                                                    GetLogLevel(),
                                                    GetLogWriterPtr(),
                                                    GetSignalBroadcast());
                    alias->RegisterObject(conn);
                    ret = build_set_property_response(property_name, alias->GetAlias());
                }
                catch (DBusException& err)
                {
                    delete alias;
                    alias = nullptr;
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_EXISTS,
                                                obj_path, intf_name, property_name,
                                                err.getRawError().c_str());
                }
            }
            else if (("name" == property_name) && conn)
            {
                gsize len = 0;
                name = std::string(g_variant_get_string(value, &len));
                ret = build_set_property_response(property_name, name);
            }
            else if (("locked_down" == property_name) && conn)
            {
                ret = properties.SetValue(property_name, value);
                LogInfo("Configuration lock-down flag set to "
                         + (locked_down ? std::string("true") : std::string("false"))
                         + " by UID " + std::to_string(GetUID(sender)));
            }
            else if (("public_access" == property_name) && conn)
            {
                bool acl_public = g_variant_get_boolean(value);
                SetPublicAccess(acl_public);
                ret = build_set_property_response(property_name, acl_public);
                LogInfo("Public access set to "
                         + (acl_public ? std::string("true") : std::string("false"))
                         + " by UID " + std::to_string(GetUID(sender)));
            }
            else if (conn && properties.Exists(property_name))
            {
                ret = properties.SetValue(property_name, value);
            }
            else
            {
                throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                            obj_path, intf_name, property_name,
                                            "Denied");
            };

            return ret;
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.getUserError());
        }
    };


    /**
     *  Sets an override value for the configuration profile
     *
     * @param key    char * of the override key
     * @param value  GVariant object of the override value to use
     *
     * @return  Returns the OverrideValue object added to the
     *          array of override settings
     */
    OverrideValue set_override(const gchar *key, GVariant *value)
    {
        const ValidOverride& vo = GetConfigOverride(key);
        if (!vo.valid())
        {
            THROW_DBUSEXCEPTION("ConfigManagerObject",
                                "Invalid override key '" + std::string(key)
                                + "'");
        }

        // Ensure that a previous override value is remove
        (void) remove_override(key);

        if (OverrideType::string == vo.type)
        {
            std::string g_type(g_variant_get_type_string(value));
            if ("s" != g_type)
            {
                THROW_DBUSEXCEPTION("ConfigManagerObject",
                                    "Invalid data type for key '"
                                    + std::string(key) + "'");
            }

            gsize len = 0;
            std::string v(g_variant_get_string(value, &len));
            override_list.push_back(OverrideValue(vo, v));

        }
        else if (OverrideType::boolean == vo.type)
        {
            std::string g_type(g_variant_get_type_string(value));
            if ("b" != g_type)
            {
                THROW_DBUSEXCEPTION("ConfigManagerObject",
                                    "Invalid data type for key '"
                                    + std::string(key) + "'");
            }

            bool v = g_variant_get_boolean(value);
            override_list.push_back(OverrideValue(vo, v));
        }
        return override_list.back();
    }


    /**
     *  Removes and override from the std::vector<OverrideValue> array
     *
     * @param key  std::string of the override key to remove
     *
     * @return Returns true on successful removal, otherwise false.
     */
    bool remove_override(const gchar *key)
    {
        for (auto it = override_list.begin(); it != override_list.end(); it++)
        {
            if ((*it).override.key == key)
            {
                override_list.erase(it);
                return true;
            }
        }
        return false;
    }


private:
    std::function<void()> remove_callback;
    std::string name;
    std::time_t import_tstamp;
    std::time_t last_use_tstamp;
    unsigned int used_count;
    bool valid;
    bool readonly;
    bool single_use;
    bool persistent;
    bool locked_down;
    ConfigurationAlias *alias;
    PropertyCollection properties;
    OptionListJSON options;
    std::vector<OverrideValue> override_list;
};


/**
 *  The ConfigManagerObject is the main object for the Configuration Manager
 *  D-Bus service.  Whenever any user (D-Bus clients) accesses the main
 *  object path, this object is invoked.
 *
 *  This object basically handles the Import method, which takes a
 *  configuration profile as input and creates a ConfigurationObject.  There
 *  will exist a ConfigurationObject for each imported profile, having its
 *  own unique D-Bus object path.  Whenever a user (D-Bus client) accesses
 *  an object, the ConfigurationObject is invoked directly by the D-Bus
 *  implementation.
 */
class ConfigManagerObject : public DBusObject,
                            public ConfigManagerSignals,
                            public RC<thread_safe_refcount>
{
public:
    typedef  RCPtr<ConfigManagerObject> Ptr;

    /**
     *  Constructor initializing the ConfigManagerObject to be registered on
     *  the D-Bus.
     *
     * @param dbuscon  D-Bus this object is tied to
     * @param objpath  D-Bus object path to this object
     * @param default_log_level  Unsigned integer defining the initial log level
     * @param logwr      Pointer to LogWriter object; can be nullptr to disable
     *                   file log.
     * @param signal_broadcast Should signals be broadcasted (true) or
     *                         targeted for the log service (false)
     *
     */
    ConfigManagerObject(GDBusConnection *dbusc, const std::string objpath,
                        unsigned int default_log_level, LogWriter *logwr,
                        bool signal_broadcast)
        : DBusObject(objpath),
          ConfigManagerSignals(dbusc, objpath, default_log_level, logwr,
                               signal_broadcast),
          dbuscon(dbusc),
          creds(dbusc)
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" + objpath + "'>"
                          << "    <interface name='" + OpenVPN3DBus_interf_configuration + "'>"
                          << "        <method name='Import'>"
                          << "          <arg type='s' name='name' direction='in'/>"
                          << "          <arg type='s' name='config_str' direction='in'/>"
                          << "          <arg type='b' name='single_use' direction='in'/>"
                          << "          <arg type='b' name='persistent' direction='in'/>"
                          << "          <arg type='o' name='config_path' direction='out'/>"
                          << "        </method>"
                          << "        <method name='FetchAvailableConfigs'>"
                          << "          <arg type='ao' name='paths' direction='out'/>"
                          << "        </method>"
                          << "        <method name='TransferOwnership'>"
                          << "           <arg type='o' name='path' direction='in'/>"
                          << "           <arg type='u' name='new_owner_uid' direction='in'/>"
                          << "        </method>"
                          << "        <property type='s' name='version' access='read'/>"
                          << GetLogIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        Debug("ConfigManagerObject registered on '" + OpenVPN3DBus_interf_configuration + "':" + objpath);
    }

    ~ConfigManagerObject()
    {
        LogVerb2("Shutting down");
        RemoveObject(dbuscon);
    }


    /**
     *  Callback method called each time a method in the
     *  ConfigurationManagerObject is called over the D-Bus.
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
        IdleCheck_UpdateTimestamp();
        if ("Import" == method_name)
        {
            // Import the configuration
            std::string cfgpath = generate_path_uuid(OpenVPN3DBus_rootp_configuration, 'x');

            auto *cfgobj = new ConfigurationObject(dbuscon,
                                                   [self=Ptr(this), cfgpath]()
                                                   {
                                                       self->remove_config_object(cfgpath);
                                                   },
                                                   cfgpath,
                                                   GetLogLevel(),
                                                   GetLogWriterPtr(),
                                                   GetSignalBroadcast(),
                                                   creds.GetUID(sender),
                                                   params);
            IdleCheck_RefInc();
            cfgobj->IdleCheck_Register(IdleCheck_Get());
            cfgobj->RegisterObject(conn);
            config_objects[cfgpath] = cfgobj;

            Debug(std::string("ConfigurationObject registered on '")
                         + intf_name + "': " + cfgpath
                         + " (owner uid " + std::to_string(creds.GetUID(sender)) + ")");
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(o)", cfgpath.c_str()));
        }
        else if ("FetchAvailableConfigs" == method_name)
        {
            // Build up an array of object paths to available config objects
            GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("ao"));
            for (auto& item : config_objects)
            {
                try {
                    // We check if the caller is allowed to access this
                    // configuration object.  If not, an exception is thrown
                    // and we will just ignore that exception and continue
                    item.second->CheckACL(sender);
                    g_variant_builder_add(bld, "o", item.first.c_str());
                }
                catch (DBusCredentialsException& excp)
                {
                    // Ignore credentials exceptions.  It means the
                    // caller does not have access this configuration object
                }
            }

            // Wrap up the result into a tuple, which GDBus expects and
            // put it into the invocation response
            GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(ret, g_variant_builder_end(bld));
            g_dbus_method_invocation_return_value(invoc,
                                                  g_variant_builder_end(ret));

            // Clean-up
            g_variant_builder_unref(bld);
            g_variant_builder_unref(ret);
        }
        else if ("TransferOwnership" == method_name)
        {
            // This feature is quite powerful and is restricted to the
            // root account only.  This is typically used by openvpn3-autoload
            // when run during boot where the auto-load configuration wants
            // the owner to be someone else than root.
            if (0 != creds.GetUID(sender))
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.acl.denied",
                                                              "Access Denied");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            gchar *cfgpath = nullptr;
            uid_t new_uid = 0;
            g_variant_get(params, "(ou)", &cfgpath, &new_uid);

            for (const auto& ci : config_objects)
            {
                if (ci.first == cfgpath)
                {
                    uid_t cur_owner = ci.second->GetOwnerUID();
                    ci.second->TransferOwnership(new_uid);
                    g_dbus_method_invocation_return_value(invoc, NULL);

                    std::stringstream msg;
                    msg << "Transfered ownership from " << cur_owner
                        << " to " << new_uid
                        << " on configuration " << cfgpath;
                    LogInfo(msg.str());
                    return;
                }
            }
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.path",
                                                          "Invalid configuration path");
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
            return;
        }
    };


    /**
     *  Callback which is used each time a ConfigManagerObject D-Bus
     *  property is being read.
     *
     *  For the ConfigManagerObject, this method will just return NULL
     *  with an error set in the GError return pointer.  The
     *  ConfigManagerObject does not use properties at all.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns always NULL, as there are no properties in the
     *          ConfigManagerObject.
     */
    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        IdleCheck_UpdateTimestamp();
        GVariant *ret = nullptr;

        if ("version" == property_name)
        {
            ret = g_variant_new_string(package_version);
        }
        else
        {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "Unknown property");
        }
        return ret;
    };


    /**
     *  Callback method which is used each time a ConfigManagerObject
     *  property is being modified over the D-Bus.
     *
     *  This will always fail with an exception, as there exists no properties
     *  which can be modified in a ConfigManagerObject.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Will always throw an exception as there are no properties to
     *         modify.
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("ConfigManagerObject", "get property not implemented");
    }


private:
    GDBusConnection *dbuscon;
    DBusConnectionCreds creds;
    std::map<std::string, ConfigurationObject *> config_objects;

    /**
     * Callback function used by ConfigurationObject instances to remove
     * its object path from the main registry of configuration objects
     *
     * @param cfgpath  std::string containing the object path to the object
     *                 to remove
     *
     */
    void remove_config_object(const std::string cfgpath)
    {
        config_objects.erase(cfgpath);
    }
};


/**
 *  Main D-Bus service implementation of the Configuration Manager.
 *
 *  This object will register the configuration manager service (destination)
 *  on the D-Bus and create a main manager object (ConfigManagerObject)
 *  which will be invoked whenever a D-Bus client (proxy) accesses this
 *  service.
 */
class ConfigManagerDBus : public DBus
{
public:
    /**
     * Constructor creating a D-Bus service for the Configuration Manager.
     *
     * @param bustype   GBusType, which defines if this service should be
     *                  registered on the system or session bus.
     * @param signal_broadcast Should signals be broadcasted (true) or
     *                         targeted for the log service (false)
     */
    ConfigManagerDBus(GDBusConnection *conn, LogWriter *logwr,
                      bool signal_broadcast)
        : DBus(conn,
               OpenVPN3DBus_name_configuration,
               OpenVPN3DBus_rootp_configuration,
               OpenVPN3DBus_interf_configuration),
          logwr(logwr),
          signal_broadcast(signal_broadcast),
          cfgmgr(nullptr),
          procsig(nullptr)
    {
        procsig.reset(new ProcessSignalProducer(conn,
                                                OpenVPN3DBus_interf_configuration,
                                                "ConfigurationManager"));
    };

    ~ConfigManagerDBus()
    {
        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
    }

    /**
     *  Sets the log level to use for the configuration manager main object
     *  and individual configuration objects.  This is essentially just an
     *  inherited value from the main program but is not something which
     *  should be changed on a per-object instance.
     *
     * @param loglvl  Log level to use
     */
    void SetLogLevel(unsigned int loglvl)
    {
        default_log_level = loglvl;
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        cfgmgr.reset(new ConfigManagerObject(GetConnection(), GetRootPath(),
                                             default_log_level, logwr,
                                             signal_broadcast));
        cfgmgr->RegisterObject(GetConnection());

        procsig->ProcessChange(StatusMinor::PROC_STARTED);

        if (nullptr != idle_checker)
        {
            cfgmgr->IdleCheck_Register(idle_checker);
        }
    };


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
        THROW_DBUSEXCEPTION("ConfigManagerDBus",
                            "openvpn3-service-configmgr could not register '"
                            + busname + "' on the D-Bus");
    };

private:
    unsigned int default_log_level = 6; // LogCategory::DEBUG
    LogWriter *logwr = nullptr;
    bool signal_broadcast = true;
    ConfigManagerObject::Ptr cfgmgr;
    ProcessSignalProducer::Ptr procsig;
};

#endif // OPENVPN3_DBUS_CONFIGMGR_HPP
