//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#pragma once

#include <ctime>
#include <set>
#include <string>
#include <vector>
#include <gdbuspp/authz-request.hpp>
#include <gdbuspp/service.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/base.hpp>

#include "log/core-dbus-logger.hpp"
#include "common/core-extensions.hpp"
#include "dbus/object-ownership.hpp"
#include "log/logwriter.hpp"
#include "configmgr-signals.hpp"
#include "overrides.hpp"


namespace ConfigManager {

/**
 *  A Configuration contains information about a specific VPN
 *  configuration profile. This object is then exposed on the
 *  D-Bus through its own unique object path.
 *
 *  The configuration manager is responsible for maintaining
 *  the life cycle of these configuration objects.
 */
class Configuration : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<Configuration>;

  public:
    /**
     *  Constructor creating a new Configuration
     *
     * @param dbuscon  D-Bus connection this object is tied to
     * @param object_manager The manager this object will remove itself from
     *                       when the Remove() method is called
     * @param creds_qry Used to retrieve method caller information
     * @param sig_configmgr Needed to signal CFG_{CREATED, DESTROYED} events
     * @param config_path D-Bus path for this object
     * @param state_dir Directory used to store persistent configurations in
     * @param name User-friendly name for the configuration
     * @param config_str A parsable string representation of the configuration
     * @param single_use If this is true, this is a one-shot configuration,
     *                   to be discarded after use
     * @param persistent If this is true, this configuration will be saved to
     *                   disk in the state_dir directory
     * @param owner UID of the process that has asked (via D-Bus) for this
     *              configuration to be created
     * @param loglevel Logging level
     * @param logwr Log helper object
     */
    Configuration(DBus::Connection::Ptr dbuscon,
                  DBus::Object::Manager::Ptr object_manager,
                  DBus::Credentials::Query::Ptr creds_qry,
                  ::Signals::ConfigurationManagerEvent::Ptr sig_configmgr,
                  const DBus::Object::Path &config_path,
                  const std::string &state_dir,
                  const std::string &name,
                  const std::string &config_str,
                  bool single_use,
                  bool persistent,
                  uid_t owner,
                  uint8_t loglevel,
                  LogWriter::Ptr logwr);

    /**
     *  Constructor loading a Configuration from persistent storage
     *
     * @param dbuscon  D-Bus connection this object is tied to
     * @param object_manager The manager this object will remove itself from
     *                       when the Remove() method is called
     * @param creds_qry Used to retrieve method caller information
     * @param sig_configmgr Needed to signal CFG_{CREATED, DESTROYED} events
     * @param filename File to save the configuration to on updates (if this
     *                 configuration is persistent)
     * @param profile JSON representation of the configuration settings
     * @param loglevel Logging level
     * @param logwr Log helper object
     */
    Configuration(DBus::Connection::Ptr dbuscon,
                  DBus::Object::Manager::Ptr object_manager,
                  DBus::Credentials::Query::Ptr creds_qry,
                  ::Signals::ConfigurationManagerEvent::Ptr sig_configmgr,
                  const std::string &filename,
                  Json::Value profile,
                  uint8_t loglevel,
                  LogWriter::Ptr logwr);

  public:
    /**
     *  Authorization callback for D-Bus object access.
     *
     * @param request   Authz::Request object
     *
     * @return true if allowed, false otherwise
     */
    const bool Authorize(const DBus::Authz::Request::Ptr authzreq) override;


    const std::string AuthorizationRejected(const DBus::Authz::Request::Ptr request) const noexcept override;

    /**
     *  Exports the configuration, including all the available settings
     *  specific to the Linux client.  The output format is JSON.
     *
     * @return Returns a Json::Value object containing the serialized
     *         configuration profile
     */
    Json::Value Export() const;

    /**
     *   Transfer ownership of this configuration object
     *
     *  @param new_owner_uid The new owner of the configuration
     */
    void TransferOwnership(uid_t new_owner_uid);

    /**
     *  Retrieve the configuration name
     *
     * @return Returns a std::string containing the configuration name
     */
    std::string GetName() const noexcept
    {
        return prop_name_;
    }

    /**
     *  Check if this configuration has a tag
     *
     * @param tag The tag to look for
     *
     * @return true if this configuration is tagged with the parameter,
     *         false otherwise
     */
    bool CheckForTag(const std::string &tag) const noexcept
    {
        return std::find(prop_tags_.begin(), prop_tags_.end(), tag) != prop_tags_.end();
    }

    /**
     *  Retrieve the configuration name
     *
     * @return Returns the UID of the owner of this configuration
     */
    uid_t GetOwnerUID() const noexcept
    {
        return object_acl_->GetOwner();
    }

    /**
     *  Retrieve the configuration name
     *
     * @param called D-Bus identifier for the caller object
     *
     * @return true if the caller is allowed to access this configuration
     */
    bool CheckACL(const std::string &caller) const noexcept;

  private:
    void add_methods();
    void add_properties();
    void update_persistent_file();

    /**
     *  Very simple validation of the configuration profile.
     *  Currently only checks if --dev, --remote, --ca and
     *  --client or --tls-client is present in the configuration profile
     *  and that the 'GENERIC_PROFILE' setting is absent ("server locked")
     *
     * @return std::string  Returns an empty string on success and sets the
     *         prop_valid_ property to true.  Otherwise a reason why it failed
     *         is returned with prop_valid_ set to false.
     */
    std::string validate_profile() noexcept;

    void method_fetch(DBus::Object::Method::Arguments::Ptr args, bool json);
    void method_add_tag(DBus::Object::Method::Arguments::Ptr args);
    void method_remove_tag(DBus::Object::Method::Arguments::Ptr args);
    void method_set_override(DBus::Object::Method::Arguments::Ptr args);
    void method_unset_override(DBus::Object::Method::Arguments::Ptr args);
    void method_access_grant(DBus::Object::Method::Arguments::Ptr args);
    void method_access_revoke(DBus::Object::Method::Arguments::Ptr args);
    void method_seal();
    void method_remove();

    /**
     *  Sets an override value for the configuration profile
     *
     * @param key    char * of the override key
     * @param value  GVariant object of the override value to use
     *
     * @return  Returns the OverrideValue object added to the
     *          array of override settings
     */
    OverrideValue set_override(const std::string &key, GVariant *value);

    /**
     *  Sets an override value for the configuration profile
     *
     * @param key    char * of the override key
     * @param value  Value for the override
     *
     * @return  Returns the OverrideValue object added to the
     *          array of override settings
     */
    template <typename T>
    OverrideValue set_override(const std::string &key, T value)
    {
        const ValidOverride &vo = GetConfigOverride(key);
        if (!vo.valid())
        {
            throw DBus::Object::Method::Exception("Invalid override key '"
                                                  + std::string(key) + "'");
        }

        // Ensure that a previous override value is removed.
        remove_override(key);

        switch (vo.type)
        {
        case OverrideType::string:
        case OverrideType::boolean:
            override_list_.push_back(OverrideValue(vo, value));
            return override_list_.back();

        default:
            throw DBus::Object::Method::Exception("Unsupported data type for key '"
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
    bool remove_override(const std::string &key);

    /**
     *  Removes and override from the std::vector<OverrideValue> array
     *
     * @param key  std::string of the override key to remove
     *
     * @return Returns true on successful removal, otherwise false.
     */
    template <typename T>
    void add_persistent_property(const char *name,
                                 const char *dbus_type,
                                 T &property_var)
    {
        AddPropertyBySpec(
            name,
            dbus_type,
            [&](const DBus::Object::Property::BySpec &prop)
            {
                return glib2::Value::Create(property_var);
            },
            [&](const DBus::Object::Property::BySpec &prop,
                GVariant *value)
            {
                property_var = glib2::Value::Get<T>(value);

                auto upd = prop.PrepareUpdate();
                upd->AddValue(property_var);

                std::ostringstream msg;
                msg << GetPath()
                    << " - Property " << prop.GetName()
                    << " changed to '" << property_var << "'";
                signals_->LogVerb2(msg.str());
                update_persistent_file();

                return upd;
            });
    }

  private:
    DBus::Object::Manager::Ptr object_manager_;
    DBus::Credentials::Query::Ptr creds_qry_;
    ::Signals::ConfigurationManagerEvent::Ptr sig_configmgr_;
    ConfigManager::Log::Ptr signals_;
    GDBusPP::Object::Extension::ACL::Ptr object_acl_;
    std::string state_dir_;
    std::string prop_name_;
    bool prop_persistent_;
    bool prop_dco_{false};
    bool prop_single_use_;
    std::vector<std::string> prop_tags_;
    bool prop_transfer_owner_session_{false};
    std::time_t prop_import_timestamp_;
    std::time_t prop_last_used_timestamp_{};
    bool prop_locked_down_{false};
    bool prop_readonly_{false};
    unsigned int prop_used_count_{0};
    bool prop_valid_{false};
    std::string persistent_file_;
    openvpn::OptionListJSON options_;
    std::vector<OverrideValue> override_list_;
};

} // namespace ConfigManager
