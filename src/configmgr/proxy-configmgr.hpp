//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//

#pragma once

#include <limits>
#include <vector>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "dbus/constants.hpp"
#include "configmgr/overrides.hpp"


class CfgMgrProxyException : public DBus::Exception
{
  public:
    CfgMgrProxyException(const std::string &errmsg)
        : DBus::Exception("Configuration Manager", errmsg)
    {
    }
};


enum class CfgMgrFeatures : std::uint32_t
{
    // clang-format off
        UNDEFINED        = 0,        //< Version not identified
        TAGS             = 1,        //< Supports configuration tags
        DEVBUILD         = std::numeric_limits<std::uint32_t>::max()  //< Development build; unreleased
    // clang-format on
};
static inline bool operator&(const CfgMgrFeatures &a, const CfgMgrFeatures &b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) > 0;
}


class OpenVPN3ConfigurationProxy
{
  public:
    OpenVPN3ConfigurationProxy(DBus::Connection::Ptr con, std::string object_path, bool force_feature_load = false)
    {
        proxy = DBus::Proxy::Client::Create(con,
                                            Constants::GenServiceName("configuration"));
        proxy_tgt = DBus::Proxy::TargetPreset::Create(
            object_path,
            Constants::GenInterface("configuration"));
        proxy_qry = DBus::Proxy::Utils::Query::Create(proxy);
        Ping();

        // Only try to ensure the configuration manager service is available
        // when accessing the main management object
        if ((Constants::GenPath("configuration") == object_path) || force_feature_load)
        {
            set_feature_flags(proxy_qry->ServiceVersion(Constants::GenPath("configuration"),
                                                        Constants::GenInterface("configuration")));
        }
    }

    void Ping() const
    {
        if(!proxy_qry->Ping())
        {
            throw DBus::Proxy::Exception(
                Constants::GenServiceName("configuration"),
                proxy_tgt->object_path,
                proxy_tgt->interface,
                "Could not reach service");
        }
    }


    const bool CheckFeatures(const CfgMgrFeatures &feat)
    {
        if (features == CfgMgrFeatures::UNDEFINED)
        {
            auto service = DBus::Proxy::Utils::Query::Create(proxy);
            set_feature_flags(service->ServiceVersion(Constants::GenPath("configuration"),
                                                      Constants::GenInterface("configuration")));
        }
        return features & feat;
    }


    std::string Import(std::string name, std::string config_blob, bool single_use, bool persistent)
    {
        GVariant *res = proxy->Call(proxy_tgt,
                                    "Import",
                                    g_variant_new("(ssbb)",
                                                  name.c_str(),
                                                  config_blob.c_str(),
                                                  single_use,
                                                  persistent));
        if (NULL == res)
        {
            throw CfgMgrProxyException("Failed to import configuration");
        }

        glib2::Utils::checkParams(__func__, res, "(o)");
        std::string ret = glib2::Value::Extract<std::string>(res, 0);
        g_variant_unref(res);

        return ret;
    }


    const bool GetPersistent() const
    {
        return proxy->GetProperty<bool>(proxy_tgt, "persistent");
    }


    const bool GetSingleUse() const
    {
        return proxy->GetProperty<bool>(proxy_tgt, "single_use");
    }


    /**
     * Retrieves a string array of configuration paths which are available
     * to the calling user
     *
     * @return A std::vector<std::string> of configuration paths
     */
    std::vector<std::string> FetchAvailableConfigs()
    {
        GVariant *res = proxy->Call(proxy_tgt, "FetchAvailableConfigs");
        if (NULL == res)
        {
            throw CfgMgrProxyException("Failed to retrieve available configurations");
        }
        std::vector<std::string> ret = glib2::Value::ExtractVector<std::string>(res, "o");
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
        GVariant *res = proxy->Call(proxy_tgt,
                                    "LookupConfigName",
                                    g_variant_new("(s)", cfgname.c_str()));
        if (nullptr == res)
        {
            throw CfgMgrProxyException("Failed to lookup configuration names");
        }
        std::vector<std::string> ret = glib2::Value::ExtractVector<std::string>(res, "o");
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
        if (!(features & CfgMgrFeatures::TAGS))
        {
            throw CfgMgrProxyException("Tags feature unavailable");
        }
        GVariant *res = proxy->Call(proxy_tgt,
                                    "SearchByTag",
                                    g_variant_new("(s)", tagname.c_str()));
        if (nullptr == res)
        {
            throw CfgMgrProxyException("Failed to search for configuration tags");
        }

        std::vector<std::string> ret = glib2::Value::ExtractVector<std::string>(res, "o");
        return ret;
    }


    /**
     *  Search for all available configuration profile owned by a specific owner.
     *  Both username and UID are valid.
     *
     * @param owner                      std::string containing the user to
     *                                  look for
     * @return std::vector<std::string> Array of the D-Bus object path
     *                                  to all found configuration objects
     */
    std::vector<std::string> SearchByOwner(const std::string &owner) const
    {
        GVariant *res = proxy->Call(proxy_tgt,
                                    "SearchByOwner",
                                    g_variant_new("(s)", owner.c_str()));
        if (nullptr == res)
        {
            throw CfgMgrProxyException("Failed to search for configuration tags");
        }

        std::vector<std::string> ret = glib2::Value::ExtractVector<std::string>(res, "o");
        return ret;
    }


    std::string GetJSONConfig()
    {
        GVariant *res = proxy->Call(proxy_tgt, "FetchJSON");
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("Failed to retrieve configuration (JSON format)");
        }

        glib2::Utils::checkParams(__func__, res, "(s)");
        std::string ret = glib2::Value::Extract<std::string>(res, 0);
        g_variant_unref(res);

        return ret;
    }


    std::string GetConfig()
    {
        GVariant *res = proxy->Call(proxy_tgt, "Fetch");
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("Failed to retrieve configuration");
        }

        glib2::Utils::checkParams(__func__, res, "(s)");
        std::string ret = glib2::Value::Extract<std::string>(res, 0);
        g_variant_unref(res);

        return ret;
    }


    void Remove()
    {
        GVariant *res = proxy->Call(proxy_tgt, "Remove");
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("Failed to delete the configuration");
        }
        g_variant_unref(res);
    }


    const std::string GetName() const
    {
        return proxy->GetProperty<std::string>(proxy_tgt, "name");
    }

    void SetName(std::string name)
    {
        proxy->SetProperty(proxy_tgt, "name", name);
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
        proxy->SetProperty(proxy_tgt, "locked_down", lockdown);
    }


    /**
     *  Retrieves the locked-down flag for the configuration profile
     *
     *  @return Returns true if the configuration is locked down.
     */
    bool GetLockedDown()
    {
        return proxy->GetProperty<bool>(proxy_tgt, "locked_down");
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
        proxy->SetProperty(proxy_tgt, "public_access", public_access);
    }


    /**
     *  Retrieve the public-access flag for the configuration profile
     *
     * @return Returns true if public-access is granted
     */
    bool GetPublicAccess()
    {
        return proxy->GetProperty<bool>(proxy_tgt, "public_access");
    }


    /**
     *  Sets the data-channel offload capability flag
     *
     * @param dco    Boolean flag.  If set to true, the kernel accelerator
     *               will be used for data channel offloading.
     */
    void SetDCO(bool dco)
    {
        proxy->SetProperty(proxy_tgt, "dco", dco);
    }


    /**
     *   Retrieve the data-channel offloading (DCO) capability setting.  If
     *   set to true, the kernel accelerator will be used.
     *
     * @return Returns true if DCO is enabled
     */
    bool GetDCO()
    {
        return proxy->GetProperty<bool>(proxy_tgt, "dco");
    }


    void Seal()
    {
        GVariant *res = proxy->Call(proxy_tgt, "Seal");
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("Failed to seal the configuration");
        }
        g_variant_unref(res);
    }


    const bool GetSealed() const
    {
        return proxy->GetProperty<bool>(proxy_tgt, "readonly");
    }


    /**
     * Grant a user ID (uid) access to this configuration profile
     *
     * @param uid  uid_t value of the user which will be granted access
     */
    void AccessGrant(uid_t uid)
    {
        GVariant *res = proxy->Call(proxy_tgt,
                                    "AccessGrant",
                                    g_variant_new("(u)", uid));
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("AccessGrant() call failed");
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
        GVariant *res = proxy->Call(proxy_tgt,
                                    "AccessRevoke",
                                    g_variant_new("(u)", uid));
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("AccessRevoke() call failed");
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
        return proxy->GetProperty<uid_t>(proxy_tgt, "owner");
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
        return proxy->GetProperty<bool>(proxy_tgt, "transfer_owner_session");
    }


    /**
     *  Sets the session ownership transfer flag for a configuration
     *  profile
     *
     * @param value Boolean value to enable/disable the setting
     */
    void SetTransferOwnerSession(const bool value)
    {
        return proxy->SetProperty(proxy_tgt, "transfer_owner_session", value);
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

        GVariant *res = proxy->GetPropertyGVariant(proxy_tgt, "overrides");
        if (NULL == res)
        {
            throw DBus::Proxy::Exception("GetProperty(\"overrides\") call failed");
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
                throw DBus::Proxy::Exception("Invalid override found");
            }
            if (OverrideType::string == vo.type)
            {
                std::string v(glib2::Value::Get<std::string>(val));
                ret.push_back(OverrideValue(vo, v));
            }
            else if (OverrideType::boolean == vo.type)
            {
                bool v = glib2::Value::Get<bool>(val);
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
            DBus::Proxy::Exception("Invalid override");
        }
        if (OverrideType::boolean != override.type)
        {
            DBus::Proxy::Exception("SetOverride for bool called for non-bool override");
        }
        GVariant *val = glib2::Value::Create(value);
        proxy->Call(proxy_tgt,
                    "SetOverride",
                    g_variant_new("(sv)", override.key.c_str(), val));
    }


    /**
     * Adds a string override to this object.
     */
    void SetOverride(const ValidOverride &override, std::string value)
    {
        if (!override.valid())
        {
            DBus::Proxy::Exception("Invalid override");
        }
        if (OverrideType::string != override.type)
        {
            DBus::Proxy::Exception("SetOverride for string called for non-string override");
        }
        GVariant *val = glib2::Value::Create(value);
        proxy->Call(proxy_tgt,
                    "SetOverride",
                    g_variant_new("(sv)", override.key.c_str(), val));
    }


    /**
     *  Adds a tag value to a configuration profile object
     *
     * @param tag std::string of the tag value to assign to the object
     */
    void AddTag(const std::string &tag) const
    {
        if (!(features & CfgMgrFeatures::TAGS))
        {
            throw CfgMgrProxyException("Tags feature unavailable");
        }
        if ("system:" == tag.substr(0, 7))
        {
            throw CfgMgrProxyException("System tags cannot be added by users");
        }

        try
        {
            proxy->Call(proxy_tgt,
                        "AddTag",
                        glib2::Value::CreateTupleWrapped(tag));
        }
        catch (const DBus::Exception &excp)
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
        if (!(features & CfgMgrFeatures::TAGS))
        {
            throw CfgMgrProxyException("Tags feature unavailable");
        }

        if ("system:" == tag.substr(0, 7))
        {
            throw CfgMgrProxyException("System tags cannot be removed by users");
        }

        try
        {
            proxy->Call(proxy_tgt,
                        "RemoveTag",
                        glib2::Value::CreateTupleWrapped(tag));
        }
        catch (const DBus::Exception &excp)
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
        if (!(features & CfgMgrFeatures::TAGS))
        {
            throw CfgMgrProxyException("Tags feature unavailable");
        }

        auto tagslist = proxy->GetPropertyArray<std::string>(proxy_tgt, "tags");
        std::vector<std::string> ret;
        for (const auto &tag : tagslist)
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
        throw DBus::Proxy::Exception("Override not found");
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
        throw DBus::Proxy::Exception("Invalid override key:" + key);
    }


    /**
     * Removes an override from this object.
     */
    void UnsetOverride(const ValidOverride &override)
    {
        if (!override.valid())
        {
            throw DBus::Proxy::Exception("Invalid override");
        }
        proxy->Call(proxy_tgt,
                    "UnsetOverride",
                    g_variant_new("(s)", override.key.c_str()));
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
        auto ret = proxy->GetPropertyArray<uid_t>(proxy_tgt, "acl");
        return ret;
    }


  private:
    DBus::Proxy::Client::Ptr proxy{nullptr};
    DBus::Proxy::TargetPreset::Ptr proxy_tgt{nullptr};
    DBus::Proxy::Utils::Query::Ptr proxy_qry{nullptr};

    CfgMgrFeatures features = CfgMgrFeatures::UNDEFINED;
    std::vector<OverrideValue> cached_overrides = {};


    void set_feature_flags(const std::string &vs)
    {
        if ('v' == vs[0])
        {
            unsigned int v = std::stoi(vs.substr(1));
            if (21 <= v)
            {
                features = CfgMgrFeatures::TAGS;
            }
        }
        else
        {
            // Development builds are expected to have all features.
            features = CfgMgrFeatures::DEVBUILD;
        }
    }
};
