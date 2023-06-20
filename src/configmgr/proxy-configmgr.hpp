//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020 - 2023  Lev Stipakov <lev@openvpn.net>
//

#pragma once

#include <vector>

#include "dbus/core.hpp"
#include "configmgr/overrides.hpp"

using namespace openvpn;


class CfgMgrProxyException : std::exception
{
  public:
    CfgMgrProxyException(const std::string &errmsg)
        : errormsg(errmsg)
    {
    }


    const char *what() const noexcept
    {
        return errormsg.c_str();
    }


  private:
    std::string errormsg = {};
};



class OpenVPN3ConfigurationProxy : public DBusProxy
{
  public:
    OpenVPN3ConfigurationProxy(GBusType bus_type, std::string object_path)
        : DBusProxy(bus_type,
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "",
                    true)
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
            (void)GetServiceVersion();
        }
    }

    OpenVPN3ConfigurationProxy(DBus &dbusobj, std::string object_path)
        : DBusProxy(dbusobj.GetConnection(),
                    OpenVPN3DBus_name_configuration,
                    OpenVPN3DBus_interf_configuration,
                    "",
                    true)
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
            (void)GetServiceVersion();
        }
    }


    std::string Import(std::string name, std::string config_blob, bool single_use, bool persistent)
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

        GLibUtils::checkParams(__func__, res, "(o)");
        std::string ret = GLibUtils::ExtractValue<std::string>(res, 0);
        g_variant_unref(res);

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
        std::vector<std::string> ret = GLibUtils::ParseGVariantList<std::string>(res, "o");
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
        std::vector<std::string> ret = GLibUtils::ParseGVariantList<std::string>(res, "o");
        return ret;
    }


    /**
     *  Search for all available configuration profile with a specific
     *  tag name.
     *
     * @param tagname                   std::string containing the tag value to
     *                                  look for
     * @return std::vector<std::string> Array of the D-Bus object path
     *                                  to all found configuration objects
     */
    std::vector<std::string> SearchByTag(const std::string &tagname) const
    {
        GVariant *res = Call("SearchByTag",
                             g_variant_new("(s)", tagname.c_str()));
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "Failed to search for configuration tags");
        }

        std::vector<std::string> ret = GLibUtils::ParseGVariantList<std::string>(res, "o");
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

        GLibUtils::checkParams(__func__, res, "(s)");
        std::string ret = GLibUtils::ExtractValue<std::string>(res, 0);
        g_variant_unref(res);

        return ret;
    }


    std::string GetConfig()
    {
        GVariant *res = Call("Fetch");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy", "Failed to retrieve configuration");
        }

        GLibUtils::checkParams(__func__, res, "(s)");
        std::string ret = GLibUtils::ExtractValue<std::string>(res, 0);
        g_variant_unref(res);

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
     *  @param clear_cache  If true the object local cache will be cleared
     *                      first (default), otherwise the cached result
     *                      will be returned
     *
     * @return A list of VpnOverride key, value pairs
     */
    std::vector<OverrideValue> GetOverrides(bool clear_cache = true)
    {
        // If we have cache available ...
        if (!cached_overrides.empty())
        {
            // ... should it be cleared?
            if (!clear_cache)
            {
                return cached_overrides;
            }

            // ... otherwise, clear it before fetching new data
            cached_overrides.clear();
        }

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

            const ValidOverride &vo = GetConfigOverride(key);
            if (!vo.valid())
            {
                THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                    "Invalid override found");
            }
            if (OverrideType::string == vo.type)
            {
                std::string v(GLibUtils::GetVariantValue<std::string>(val));
                ret.push_back(OverrideValue(vo, v));
            }
            else if (OverrideType::boolean == vo.type)
            {
                bool v = GLibUtils::GetVariantValue<bool>(val);
                ret.push_back(OverrideValue(vo, v));
            }
        }
        cached_overrides = ret;
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
        GVariant *val = g_variant_new("b", value);
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
        GVariant *val = g_variant_new("s", value.c_str());
        Call("SetOverride", g_variant_new("(sv)", override.key.c_str(), val));
    }


    /**
     *  Adds a tag value to a configuration profile object
     *
     * @param tag std::string of the tag value to assign to the object
     */
    void AddTag(const std::string &tag) const
    {
        if ("system:" == tag.substr(0, 7))
        {
            throw CfgMgrProxyException("System tags cannot be added by users");
        }

        try
        {
            Call("AddTag", g_variant_new("(s)", tag.c_str()));
        }
        catch (const DBusException &excp)
        {
            std::stringstream e;
            e << excp.GetRawError() << ", tag value: " << tag;
            throw CfgMgrProxyException(e.str().substr(e.str().find(":") + 1));
        }
    }


    /**
     *  Removes a tag value from a configuration profile object
     *
     * @param tag std::string of the tag value to remove from the object
     */
    void RemoveTag(const std::string &tag) const
    {
        if ("system:" == tag.substr(0, 7))
        {
            throw CfgMgrProxyException("System tags cannot be removed by users");
        }

        try
        {
            Call("RemoveTag", g_variant_new("(s)", tag.c_str()));
        }
        catch (const DBusException &excp)
        {
            std::stringstream e;
            e << excp.GetRawError() << ", tag value: " << tag;
            throw CfgMgrProxyException(e.str().substr(e.str().find(":") + 1));
        }
    }


    /**
     * Retrieve a an array of strings of tags assigned to the
     * configuration profile object
     *
     * @return const std::vector<std::string> containing all tags
     */
    const std::vector<std::string> GetTags() const
    {
        GVariant *res = GetProperty("tags");
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3ConfigurationProxy",
                                "GetTags() call failed");
        }

        std::vector<std::string> ret;
        for (const auto &tag : GLibUtils::ParseGVariantList<std::string>(res, "s", false))
        {
            if ("system:" != tag.substr(0, 7))
            {
                ret.push_back(tag);
            }
        }
        return ret;
    }


    /**
     *  Retrieve an OverrideValue object for a specific configuration
     *  profile override.
     *
     * @param key    std::string of the override key to look up
     * @return const OverrideValue& to the override value object
     * @throws DBusException if the override key was not found
     */
    const OverrideValue &GetOverrideValue(const std::string &key)
    {
        if (cached_overrides.empty())
        {
            (void)GetOverrides(true);
        }

        for (const auto &ov : cached_overrides)
        {
            if (ov.override.key == key)
            {
                return ov;
            }
        }
        THROW_DBUSEXCEPTION("OpenVPNConfigurationProxy", "Override not found");
    }


    const ValidOverride &LookupOverride(const std::string key)
    {
        for (const ValidOverride &vo : configProfileOverrides)
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
        std::vector<uid_t> ret = GLibUtils::ParseGVariantList<uid_t>(res, "u", false);
        return ret;
    }

  private:
    std::vector<OverrideValue> cached_overrides = {};
};
