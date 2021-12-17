//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2021  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2021  David Sommerseth <davids@openvpn.net>
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
#include "common/cmdargparser-exceptions.hpp"
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


    virtual ~ConfigManagerSignals() = default;


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
                        uid_t creator, std::string state_dir, GVariant *params)
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
          dco(false),
          properties(this),
          persistent_file("")
    {
        GLibUtils::checkParams(__func__, params, "(ssbb)", 4);
        name = GLibUtils::ExtractValue<std::string>(params, 0);
        std::string cfgstr(GLibUtils::ExtractValue<std::string>(params, 1));
        single_use = GLibUtils::ExtractValue<bool>(params, 2);
        bool persistent = GLibUtils::ExtractValue<bool>(params, 3);

        // Parse the options from the imported configuration
        OptionList::Limits limits("profile is too large",
                                  ProfileParseLimits::MAX_PROFILE_SIZE,
                                  ProfileParseLimits::OPT_OVERHEAD,
                                  ProfileParseLimits::TERM_OVERHEAD,
                                  ProfileParseLimits::MAX_LINE_SIZE,
                                  ProfileParseLimits::MAX_DIRECTIVE_SIZE);
        options.parse_from_config(cfgstr, &limits);
        initialize_configuration(persistent);

        if (persistent && !state_dir.empty())
        {
            std::string objp = GetObjectPath();
            std::stringstream p;
            p << state_dir << "/" << simple_basename(objp) << ".json";
            persistent_file = std::string(p.str());
            update_persistent_file();
        }
    }

    ConfigurationObject(GDBusConnection *dbuscon,
                        const std::string& fname, Json::Value profile,
                        std::function<void()> remove_callback,
                        unsigned int default_log_level,
                        LogWriter *logwr, bool signal_broadcast)
        : DBusObject(profile["object_path"].asString()),
          ConfigManagerSignals(dbuscon, profile["object_path"].asString(),
                               default_log_level, logwr, signal_broadcast),
          DBusCredentials(dbuscon, profile["owner"].asUInt64()),
          remove_callback(remove_callback),
          properties(this),
          persistent_file(fname)
    {
        name = profile["name"].asString();
        import_tstamp = profile["import_timestamp"].asUInt64();
        last_use_tstamp = profile["last_used_timestamp"].asUInt64();
        locked_down = profile["locked_down"].asBool();
        transfer_owner_sess = profile["transfer_owner_session"].asBool();
        readonly = profile["readonly"].asBool();
        single_use = profile["single_use"].asBool();
        used_count = profile["used_count"].asUInt();
        dco = profile["dco"].asBool();
        valid = profile["valid"].asBool();

        SetPublicAccess(profile["public_access"].asBool());
        for (const auto& id : profile["acl"])
        {
            GrantAccess(id.asUInt());
        }

        for (const auto& ovkey : profile["overrides"].getMemberNames())
        {
            Json::Value ov = profile["overrides"][ovkey];
            switch(ov.type())
            {
            case Json::ValueType::booleanValue:
                set_override(ovkey.c_str(), ov.asBool());
                break;
            case Json::ValueType::stringValue:
                set_override(ovkey.c_str(), ov.asString());
                break;
            default:
                THROW_DBUSEXCEPTION("ConfigurationObject",
                                    "Invalid data type for ...");
            }
        }

        // Parse the options from the imported configuration
        options.json_import(profile["profile"]);

        initialize_configuration(true);
    }

    ~ConfigurationObject()
    {
        remove_callback();
        Debug("Configuration removed");
        IdleCheck_RefDec();
    };


    /**
     *  Retrieve the configuration name
     *
     * @return Returns a std::string containing the configuration name
     */
    std::string GetConfigName() const noexcept
    {
        return name;
    }


    /**
     *  Exports the configuration, including all the available settings
     *  specific to the Linux client.  The output format is JSON.
     *
     * @return Returns a Json::Value object containing the serialized
     *         configuration profile
     */
    Json::Value Export() const
    {
        Json::Value ret;

        ret["object_path"] = GetObjectPath();
        ret["owner"] = (uint32_t) GetOwnerUID();
        ret["name"] = name;
        ret["import_timestamp"] = (uint32_t) import_tstamp;
        ret["last_used_timestamp"] = (uint32_t) last_use_tstamp;
        ret["locked_down"] = locked_down;
        ret["transfer_owner_session"] = transfer_owner_sess;
        ret["readonly"] = readonly;
        ret["single_use"] = single_use;
        ret["used_count"] = used_count;
        ret["valid"] = valid;
        ret["profile"] = options.json_export();
        ret["dco"] = dco;

        ret["public_access"] = GetPublicAccess();
        for (const auto& e : GetAccessList())
        {
            ret["acl"].append(e);
        }
        for (const auto& ov : override_list)
        {
            switch (ov.override.type)
            {
            case OverrideType::boolean:
                ret["overrides"][ov.override.key] = ov.boolValue;
                break;

            case OverrideType::string:
                ret["overrides"][ov.override.key] = ov.strValue;
                break;

            default:
                THROW_DBUSEXCEPTION("ConfigurationObject",
                                    "Invalid override type for key "
                                    "'" + ov.override.key + "'");
            }
        }

        return ret;
    }


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
                // configuration to be "used".
                //
                // If we don't have an UID for some reason, don't remove
                // anything.
                //
                try
                {
                    uid_t ovpn_uid;
                    try
                    {
                        ovpn_uid = lookup_uid(OPENVPN_USERNAME);
                    }
                    catch (const LookupException& excp)
                    {
                        excp.SetDBusError(invoc);
                        return;
                    }

                    if (GetUID(sender) == ovpn_uid)
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
                        update_persistent_file();
                    }
                }
                catch (DBusException& excp)
                {
                    std::string err(excp.what());
                    if (err.find("NameHasNoOwner: Could not get UID of name") == std::string::npos)
                    {
                        // If the error is related to something else than
                        // retriving the UID, re-throw the exception
                        throw;
                    }
                }
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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

                std::stringstream jsoncfg;
                jsoncfg << options.json_export();

                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(s)",
                                                                    jsoncfg.str().c_str()));

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
                LogWarn(excp.what());
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
                update_persistent_file();
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
                update_persistent_file();
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
                update_persistent_file();
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
                update_persistent_file();
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
                update_persistent_file();
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
                    update_persistent_file();
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
                LogWarn(excp.what());
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

                // If this is a persistent config, remove it from
                // the file system too
                if (!persistent_file.empty())
                {
                    unlink(persistent_file.c_str());
                    LogVerb2("Persistent configuration profile removed: '"
                             + persistent_file + "'");
                }
                delete this;
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.what());
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
            else if ("public_access" == property_name)
            {
                ret = g_variant_new_boolean(GetPublicAccess());
            }
            else if ("acl" == property_name)
            {
                    ret = GLibUtils::GVariantFromVector(GetAccessList());
            }
            else if ("persistent" == property_name)
            {
                ret = g_variant_new_boolean(!persistent_file.empty());
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
            LogWarn(excp.what());
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
            if (("name" == property_name) && conn)
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

            update_persistent_file();
            return ret;
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.what());
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
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
            THROW_DBUSEXCEPTION("ConfigurationObject",
                                "Invalid override key '" + std::string(key)
                                + "'");
        }

        // Ensure that a previous override value is remove
        (void) remove_override(key);

        std::string g_type(g_variant_get_type_string(value));
        if ("s" == g_type)
        {
            gsize len = 0;
            std::string v(g_variant_get_string(value, &len));
            return set_override(key, v);
        }
        else if ("b" == g_type)
        {
            return set_override(key, g_variant_get_boolean(value));
        }
        THROW_DBUSEXCEPTION("ConfigurationObject",
                            "Unsupported override data type: " + g_type);
    }


    /**
     *  Sets an override value for the configuration profile
     *
     * @param key    char * of the override key
     * @param value  Value for the override
     *
     * @return  Returns the OverrideValue object added to the
     *          array of override settings
     */
    template<typename T>
    OverrideValue set_override(const gchar *key, T value)
    {
        const ValidOverride& vo = GetConfigOverride(key);
        if (!vo.valid())
        {
            THROW_DBUSEXCEPTION("ConfigurationObject",
                                "Invalid override key '" + std::string(key)
                                + "'");
        }

        // Ensure that a previous override value is remove
        (void) remove_override(key);

        switch (vo.type)
        {
        case OverrideType::string:
        case OverrideType::boolean:
            override_list.push_back(OverrideValue(vo, value));
            return override_list.back();

        default:
            THROW_DBUSEXCEPTION("ConfigurationObject",
                                "Unsupported data type for key '"
                                + std::string(key) + "'");
        }
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


    void initialize_configuration(const bool persistent)
    {
        std::stringstream msg;
        msg << "Parsed"
            << (persistent ? " persistent" : "")
            << (persistent && single_use ? "," : "")
            << (single_use ? " single-use" : "")
            << " configuration '" << name << "'"
            << ", owner: " << lookup_username(GetOwnerUID());
        LogInfo(msg.str());

        // FIXME:  Validate the configuration file, ensure --ca/--key/--cert/--dh/--pkcs12
        //         contains files
        valid = true;

        properties.AddBinding(new PropertyType<std::time_t>(this, "import_timestamp", "read", false, import_tstamp, "t"));
        properties.AddBinding(new PropertyType<std::time_t>(this, "last_used_timestamp", "read", false, last_use_tstamp, "t"));
        properties.AddBinding(new PropertyType<bool>(this, "locked_down", "readwrite", false, locked_down));
        properties.AddBinding(new PropertyType<bool>(this, "readonly", "read", false, readonly));
        properties.AddBinding(new PropertyType<bool>(this, "single_use", "read", false, single_use));
        properties.AddBinding(new PropertyType<unsigned int>(this, "used_count", "read", false, used_count));
        properties.AddBinding(new PropertyType<bool>(this, "transfer_owner_session", "readwrite", true, transfer_owner_sess));
        properties.AddBinding(new PropertyType<bool>(this, "valid", "read", false, valid));
        properties.AddBinding(new PropertyType<decltype(override_list)>(this, "overrides", "read", true, override_list));
        properties.AddBinding(new PropertyType<bool>(this, "dco", "readwrite", true, dco));

        std::string introsp_xml ="<node name='" + GetObjectPath() + "'>"
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
            "        <property type='b' name='persistent' access='read'/>"
            + properties.GetIntrospectionXML() +
            "    </interface>"
            "</node>";
        ParseIntrospectionXML(introsp_xml);
    }


    void update_persistent_file()
    {
        if (persistent_file.empty())
        {
            // If this configuration is not configured as persistent,
            // we're done.
            return;
        }

        std::ofstream state(persistent_file);
        state << Export();
        state.close();
        LogVerb2("Updated persistent config: " + persistent_file);
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
    bool locked_down;
    bool transfer_owner_sess = false;
    bool dco; // data channel offload
    PropertyCollection properties;
    std::string persistent_file;
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
                          << "        <method name='LookupConfigName'>"
                          << "          <arg type='s' name='config_name' direction='in'/>"
                          << "          <arg type='ao' name='config_paths' direction='out'/>"
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
     *  Sets the directory where the configuration manager should store
     *  persistent configuration profiles.
     *
     *  When calling this function, all already saved configuration files
     *  will be imported and registered before continuing.
     *
     * @param stdir  std::string containing the file system directory for
     *               the persistent configuration profile storage
     */
    void SetStateDirectory(const std::string& stdir)
    {
        if (!state_dir.empty())
        {
            THROW_DBUSEXCEPTION("ConfigManagerObject",
                                "State directory already set");
        }
        state_dir = stdir;

        // Load all the already saved persistent configurations before
        // continuing.
        for (const auto& fname : get_persistent_config_file_list(state_dir))
        {
            try
            {
                import_persistent_configuration(fname);
            }
            catch (const openvpn::option_error& excp)
            {
                LogCritical("Could not import configuration profile " +
                            fname + ": " + std::string(excp.what()));
            }
            catch (const DBusException& excp)
            {
                std::string err(excp.what());
                if (err.find("failed: An object is already exported for the interface") != std::string::npos)
                {
                    LogCritical("Could not import persistent configuration: " + fname);
                }
                else
                {
                    throw;
                }
            }
            catch (const std::exception& excp)
            {
                LogCritical("Invalid persistent configuration file " + fname
                            + ": " + std::string(excp.what()));
            }
        }
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
            ConfigurationObject *cfgobj;

            try
            {
                cfgobj = new ConfigurationObject(dbuscon,
                                                 [self=Ptr(this), cfgpath]()
                                                 {
                                                    self->remove_config_object(cfgpath);
                                                 },
                                                 cfgpath,
                                                 GetLogLevel(),
                                                 GetLogWriterPtr(),
                                                 GetSignalBroadcast(),
                                                 creds.GetUID(sender),
                                                 state_dir,
                                                 params);

                register_config_object(cfgobj, "created");
                g_dbus_method_invocation_return_value(invoc, g_variant_new("(o)", cfgpath.c_str()));
            }
            catch (const openvpn::option_error& excp)
            {
                std::string em{"Invalid configuration profile: "};
                em += std::string(excp.what());
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.import",
                                                              em.c_str());
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
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
        else if ("LookupConfigName" == method_name)
        {
            gchar *cfgname_c = nullptr;
            g_variant_get(params, "(s)", &cfgname_c);

            if (nullptr == cfgname_c || strlen(cfgname_c) < 1)
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.name",
                                                              "Invalid configuration name");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            std::string cfgname(cfgname_c);
            g_free(cfgname_c);

            // Build up an array of object paths to available config objects
            GVariantBuilder *found_paths = g_variant_builder_new(G_VARIANT_TYPE("ao"));
            for (const auto& item : config_objects)
            {
                if (item.second->GetConfigName() == cfgname)
                {
                    try
                    {
                        // We check if the caller is allowed to access this
                        // configuration object.  If not, an exception is thrown
                        // and we will just ignore that exception and continue
                        item.second->CheckACL(sender);
                        g_variant_builder_add(found_paths,
                                              "o", item.first.c_str());
                    }
                    catch (DBusCredentialsException& excp)
                    {
                        // Ignore credentials exceptions.  It means the
                        // caller does not have access this configuration object
                    }
                }
            }
            g_dbus_method_invocation_return_value(invoc, GLibUtils::wrapInTuple(found_paths));
            return;
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
            ret = g_variant_new_string(package_version());
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
    std::string state_dir;
    std::map<std::string, ConfigurationObject *> config_objects;


    /**
     *  Register a new configuration object on the D-Bus, with the
     *  idle-check reference counting, internal object tracking and
     *  logging handled.
     *
     * @param cfgobj    ConfigurationObject pointer to register
     * @param operation std::string used for logging, describing how it
     *                  was processed.
     */
    void register_config_object(ConfigurationObject *cfgobj,
                                const std::string& operation)
    {
        IdleCheck_RefInc();
        cfgobj->IdleCheck_Register(IdleCheck_Get());
        cfgobj->RegisterObject(dbuscon);
        config_objects[cfgobj->GetObjectPath()] = cfgobj;

        Debug("New configuration object " + operation + ": "
              + cfgobj->GetObjectPath()
              + " (owner uid " + std::to_string(cfgobj->GetOwnerUID()) + ")");
    }


    /**
     *  Get a list (std::vector<std::string>) of all persistent configuration
     *  files in the given directory.
     *
     *  The filtering is based on a course filename check.  Filenames must be
     *  41 characters long and must end with '.json'.  Only files and
     *  symbolic links are reported back.  It will not traverse any
     *  directories.
     *
     * @param directory  std::string of the directory where to find files
     *
     * @return  Returns a std::vector<std::string> of all matching files.
     *          Each entry will have the full path to the file.
     *
     * @throws  Throws DBusException if the directory cannot be opened
     */
    std::vector<std::string> get_persistent_config_file_list(const std::string& directory)
    {
        DIR *dirfd = nullptr;
        struct dirent *entry = nullptr;

        dirfd = opendir(directory.c_str());
        if (nullptr == dirfd)
        {
            std::stringstream err;
            err << "Cannot open state-dir directory '" << directory << "': "
                << strerror(errno);
            THROW_DBUSEXCEPTION("ConfigManagerObject", err.str());
        }

        std::vector<std::string> filelist;
        while (nullptr != (entry = readdir(dirfd)))
        {
            // Filter out filenames not relevant.  The expected format is:
            //    34eea818xe578x4356x924bx9fccbbeb92eb.json
            // which, for simplicity is checked as:
            //     string length == 41 and last 5 characters are ".json"
            std::string fname(entry->d_name);
            if (41 != fname.size()
               || ".json" != fname.substr(36, 5))
            {
                continue;
            }

            std::stringstream fullpath;
            fullpath <<  directory << "/" << fname;

            // Filter out only files and symbolic links
            struct stat stbuf;
            if (0 == lstat(fullpath.str().c_str(), &stbuf))
            {
                switch (stbuf.st_mode & S_IFMT)
                {
                case S_IFREG:
                case S_IFLNK:
                    filelist.push_back(fullpath.str());
                    break;

                case S_IFDIR:
                    // Ignore directories silently
                    break;

                default:
                    LogWarn("Unsupported file type: " + fullpath.str());
                    break;
                }
            }
            else
            {
                std::stringstream err;
                err << "Could not access file '" << fullpath.str() << "': "
                    << strerror(errno);
                LogError(err.str());
            }
        }
        closedir(dirfd);

        return filelist;
    }


    /**
     *  Loads a persistent configuration file from the file system.  It
     *  will register the configuration with the D-Bus path provided in
     *  the file.
     *
     *  The file must be a JSON formatted text file based on the file
     *  format generated by @ConfigurationObject::Export()
     *
     * @param fname  std::string with the filename to import
     */
    void import_persistent_configuration(const std::string& fname)
    {
        LogVerb1("Loading persistent configuration: " + fname);

        // Load the JSON file and parse it
        std::ifstream statefile(fname, std::ifstream::binary);
        Json::Value data;
        statefile >> data;
        statefile.close();

        // Extract the configuration path and prepare the
        // remove callback function required to create the
        // configuration object
        std::string cfgpath = data["object_path"].asString();
        auto remove_cb = [self=Ptr(this), cfgpath]()
                           {
                              self->remove_config_object(cfgpath);
                           };

        // Create the internal representation of the configuration,
        // which is used when registering the configuration on the D-Bus
        ConfigurationObject *cfgobj;
        cfgobj = new ConfigurationObject(dbuscon,
                                         fname,
                                         data,
                                         remove_cb,
                                         GetLogLevel(),
                                         GetLogWriterPtr(),
                                         GetSignalBroadcast());

        // Register the configuration object in this D-Bus service
        register_config_object(cfgobj, "loaded");
    }


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
     *  Enables the persistent storage feature.  This defines where
     *  these configurations will saved on the file system.
     *
     * @param stdir  std::string containing the directory where to load and
     *               save persistent configuration profiles.
     */
    void SetStateDirectory(const std::string& stdir)
    {
        state_dir = stdir;
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

        if (!state_dir.empty())
        {
            try
            {
                cfgmgr->SetStateDirectory(state_dir);
            }
            catch (const DBusException& excp)
            {
                LogEvent ev{LogGroup::CONFIGMGR,
                            LogCategory::CRIT,
                            std::string("Error parsing --state-dir: ")
                            + std::string(excp.what())};
                if (logwr)
                {
                    logwr->Write(ev);
                }
                throw ConfigFileException(ev.message);
            }
        }
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
    std::string state_dir = "";
    ConfigManagerObject::Ptr cfgmgr;
    ProcessSignalProducer::Ptr procsig;
};

#endif // OPENVPN3_DBUS_CONFIGMGR_HPP
