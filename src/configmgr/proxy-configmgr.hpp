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

#ifndef OPENVPN3_DBUS_PROXY_CONFIG_HPP
#define OPENVPN3_DBUS_PROXY_CONFIG_HPP

#include <vector>

#include "dbus/core.hpp"
#include "configmgr/overrides.hpp"

using namespace openvpn;

class OpenVPN3ConfigurationProxy : public DBusProxy {
public:
    OpenVPN3ConfigurationProxy(GBusType bus_type, std::string object_path)
        : DBusProxy(bus_type,
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "", true)
    {
        proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                           OpenVPN3DBus_interf_configuration,
                           object_path);
        property_proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                                    "org.freedesktop.DBus.Properties",
                                    object_path);

        // Only try to ensure the configuration manager service is available
        // when accessing the main management object
        if (OpenVPN3DBus_rootp_configuration == object_path)
        {
            (void) GetServiceVersion();
        }
    }

    OpenVPN3ConfigurationProxy(DBus& dbusobj, std::string object_path)
        : DBusProxy(dbusobj.GetConnection(),
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "", true)
    {
        proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                           OpenVPN3DBus_interf_configuration,
                           object_path);
        property_proxy = SetupProxy(OpenVPN3DBus_name_configuration,
                                    "org.freedesktop.DBus.Properties",
                                    object_path);
        // Only try to ensure the configuration manager service is available
        // when accessing the main management object
        if (OpenVPN3DBus_rootp_configuration == object_path)
        {
            (void) GetServiceVersion();
        }
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

        gchar *buf = nullptr;
        g_variant_get(res, "(o)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }


    /**
     * Retrieves a string array of configuration paths which are available
     * to the calling user
     *
     * @return A std::vector<std::string> of configuration paths
     */
    std::vector<std::string> FetchAvailableConfigs()
    {
        GVariant *res = Call("FetchAvailableConfigs");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to retrieve available configurations");
        }
        GVariantIter *cfgpaths = NULL;
        g_variant_get(res, "(ao)", &cfgpaths);

        GVariant *path = NULL;
        std::vector<std::string> ret;
        while ((path = g_variant_iter_next_value(cfgpaths)))
        {
            gsize len;
            ret.push_back(std::string(g_variant_get_string(path, &len)));
            g_variant_unref(path);
        }
        g_variant_unref(res);
        g_variant_iter_free(cfgpaths);
        return ret;
    }


    /**
     *  Lookup the configuration paths for a given configuration name.
     *
     * @param cfgname  std::string containing the configuration name to
     *                 look up
     *
     * @return Returns a std::vector<std::string> with all configuration
     *         paths with the given configuration name.  If no match is found,
     *         the std::vector will be empty.
     */
    std::vector<std::string> LookupConfigName(std::string cfgname)
    {
        GVariant *res = Call("LookupConfigName",
                             g_variant_new("(s)", cfgname.c_str()));
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to lookup configuration names");
        }
        GVariantIter *cfgpaths = nullptr;
        g_variant_get(res, "(ao)", &cfgpaths);

        GVariant *path = nullptr;
        std::vector<std::string> ret;
        while ((path = g_variant_iter_next_value(cfgpaths)))
        {
            ret.push_back(GLibUtils::GetVariantValue<std::string>(path));
            g_variant_unref(path);
        }
        g_variant_unref(res);
        g_variant_iter_free(cfgpaths);
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

        gchar *buf = nullptr;
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

        gchar *buf = nullptr;
        g_variant_get(res, "(s)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }

    void Remove()
    {
        GVariant *res = Call("Remove");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to delete the configuration");
        }
        g_variant_unref(res);
    }

    void SetName(std::string name)
    {
        SetProperty("name", name);
    }


    /**
     *  Lock down the configuration profile.  This removes the possibility
     *  for other users than the owner to retrieve the configuration profile
     *  in clear-text or JSON.  The exception is the root user account,
     *  which the openvpn3-service-client process runs as and needs to
     *  be able to retrieve the configuration for the VPN connection.
     *
     * @param lockdown  Boolean flag, if set to true the configuration is
     *                  locked down
     */
    void SetLockedDown(bool lockdown)
    {
        SetProperty("locked_down", lockdown);
    }


    /**
     *  Retrieves the locked-down flag for the configuration profile
     *
     *  @return Returns true if the configuration is locked down.
     */
    bool GetLockedDown()
    {
        return GetBoolProperty("locked_down");
    }


    /**
     *  Manipulate the public-access flag.  When public-access is set to
     *  true, everyone have read access to this configuration profile
     *  regardless of how the access list is configured.
     *
     *  @param public_access Boolean flag.  If set to true, everyone is
     *                       granted read access to the configuration.
     */
    void SetPublicAccess(bool public_access)
    {
        SetProperty("public_access", public_access);
    }


    /**
     *  Retrieve the public-access flag for the configuration profile
     *
     * @return Returns true if public-access is granted
     */
    bool GetPublicAccess()
    {
        return GetBoolProperty("public_access");
    }


    /**
     *  Sets the data-channel offload capability flag
     *
     * @param dco    Boolean flag.  If set to true, the kernel accelerator
     *               will be used for data channel offloading.
     */
    void SetDCO(bool dco)
    {
        SetProperty("dco", dco);
    }


    /**
     *   Retrieve the data-channel offloading (DCO) capability setting.  If
     *   set to true, the kernel accelerator will be used.
     *
     * @return Returns true if DCO is enabled
     */
    bool GetDCO()
    {
        return GetBoolProperty("dco");
    }


    void Seal()
    {
        GVariant *res = Call("Seal");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to seal the configuration");
        }
        g_variant_unref(res);
    }


    /**
     * Grant a user ID (uid) access to this configuration profile
     *
     * @param uid  uid_t value of the user which will be granted access
     */
    void AccessGrant(uid_t uid)
    {
        GVariant *res = Call("AccessGrant", g_variant_new("(u)", uid));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "AccessGrant() call failed");
        }
        g_variant_unref(res);
    }


    /**
     * Revoke the access from a user ID (uid) for this configuration profile
     *
     * @param uid  uid_t value of the user which will get access revoked
     */
    void AccessRevoke(uid_t uid)
    {
        GVariant *res = Call("AccessRevoke", g_variant_new("(u)", uid));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "AccessRevoke() call failed");
        }
        g_variant_unref(res);
    }


    /**
     *  Retrieve the owner UID of this configuration object
     *
     * @return Returns uid_t of the configuration object owner
     */
    uid_t GetOwner()
    {
        return GetUIntProperty("owner");
    }


    /**
     *  Should the ownership of the configuration profile be
     *  transfered to newly created sessions?
     *
     *  This means it will be the owner of the configuration profile
     *  who will own the VPN session, not the user starting the session
     *
     * @return  Returns true if ownership should be transferred
     */
    bool GetTransferOwnerSession()
    {
        return GetBoolProperty("transfer_owner_session");
    }


    /**
     *  Sets the session ownership transfer flag for a configuration
     *  profile
     *
     * @param value Boolean value to enable/disable the setting
     */
    void SetTransferOwnerSession(const bool value)
    {
        return SetProperty("transfer_owner_session", value);
    }


    /**
     *  Retrieve the list of overrides for this object. The overrides
     *  are key, value pairs
     *
     * @return A list of VpnOverride key, value pairs
     */
    std::vector<OverrideValue> GetOverrides()
    {
        GVariant *res = GetProperty("overrides");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "GetProperty(\"overrides\") call failed");
        }
        GVariantIter *override_iter = NULL;
        g_variant_get(res, "a{sv}", &override_iter);

        std::vector<OverrideValue> ret;

        GVariant *override;
        while ((override = g_variant_iter_next_value(override_iter)))
        {
            gchar *key = nullptr;
            GVariant *val = nullptr;
            g_variant_get(override, "{sv}", &key, &val);


            const ValidOverride& vo = GetConfigOverride(key);
            if (!vo.valid())
            {
                THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                    "Invalid override found");
            }
            if (OverrideType::string == vo.type)
            {
                gsize len = 0;
                std::string v(g_variant_get_string(val, &len));
                ret.push_back(OverrideValue(vo, v));

            }
            else if (OverrideType::boolean == vo.type)
            {
                bool v = g_variant_get_boolean(val);
                ret.push_back(OverrideValue(vo, v));
            }
        }
        g_variant_unref(res);
        g_variant_iter_free(override_iter);
        return ret;
    }

    /**
     * Adds a bool override to this object.
     */
    void SetOverride(const ValidOverride &override, bool value)
    {
        if (!override.valid())
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Invalid override");
        }
        if (OverrideType::boolean != override.type)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "SetOverride for bool called for non-bool override");
        }
        GVariant* val = g_variant_new("b", value);
        Call("SetOverride", g_variant_new("(sv)", override.key.c_str(), val));
    }


    /**
     * Adds a string override to this object.
     */
    void SetOverride(const ValidOverride &override, std::string value)
    {
        if (!override.valid())
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Invalid override");
        }
        if (OverrideType::string != override.type)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "SetOverride for string called for non-string override");
        }
        GVariant* val = g_variant_new("s", value.c_str());
        Call("SetOverride", g_variant_new("(sv)", override.key.c_str(), val));
    }


    const ValidOverride& LookupOverride(const std::string key)
    {
        for (const ValidOverride& vo: configProfileOverrides)
        {
            if (vo.key == key)
            {
                return vo;
            }
        }
        THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                            "Invalid override key:" + key);
    }

    /**
     * Removes an override from this object.
     */
    void UnsetOverride(const ValidOverride &override)
    {
        if (!override.valid())
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Invalid override");
        }
        Call("UnsetOverride", g_variant_new("(s)", override.key.c_str()));
    }


    /**
     *  Retrieve the complete access control list (acl) for this object.
     *  The acl is essentially just an array of user ids (uid)
     *
     * @return Returns an array if uid_t references for each user granted
     *         access.
     */
    std::vector<uid_t> GetAccessList()
    {
        GVariant *res = GetProperty("acl");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "GetAccessList() call failed");
        }
        GVariantIter *acl = NULL;
        g_variant_get(res, "au", &acl);

        GVariant *uid = NULL;
        std::vector<uid_t> ret;
        while ((uid = g_variant_iter_next_value(acl)))
        {
            ret.push_back(g_variant_get_uint32(uid));
            g_variant_unref(uid);
        }
        g_variant_unref(res);
        g_variant_iter_free(acl);
        return ret;
    }
};

#endif // OPENVPN3_DBUS_PROXY_CONFIG_HPP
