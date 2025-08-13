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

#include "build-config.h"

#include <cstdio>
#include <ctime>
#include <set>
#include <fmt/ranges.h>

#include "common/lookup.hpp"
#include "common/string-utils.hpp"
#include "common/utils.hpp"
#include "configmgr-configuration.hpp"
#include "constants.hpp"


namespace ConfigManager {

Configuration::Configuration(DBus::Connection::Ptr dbuscon,
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
                             LogWriter::Ptr logwr)
    : DBus::Object::Base(config_path, INTERFACE_CONFIGMGR),
      object_manager_(object_manager), creds_qry_(creds_qry),
      sig_configmgr_(sig_configmgr), state_dir_(state_dir),
      prop_name_(filter_ctrl_chars(name, true)),
      prop_persistent_(persistent), prop_single_use_(single_use),
      prop_import_timestamp_(std::time(nullptr))
{
    using namespace openvpn;
    using namespace std::string_literals;

    DisableIdleDetector(persistent);

    signals_ = ConfigManager::Log::Create(dbuscon,
                                          LogGroup::CONFIGMGR,
                                          GetPath(),
                                          logwr);
    signals_->SetLogLevel(loglevel);

    // Prepare the object handling access control lists
    object_acl_ = GDBusPP::Object::Extension::ACL::Create(dbuscon, owner);

    // Parse the options from the imported configuration
    std::string cfgcontent = filter_ctrl_chars(config_str, false);
    OptionList::Limits limits("profile is too large",
                              ProfileParseLimits::MAX_PROFILE_SIZE,
                              ProfileParseLimits::OPT_OVERHEAD,
                              ProfileParseLimits::TERM_OVERHEAD,
                              ProfileParseLimits::MAX_LINE_SIZE,
                              ProfileParseLimits::MAX_DIRECTIVE_SIZE);
    options_.parse_from_config(cfgcontent, &limits);
    options_.parse_meta_from_config(cfgcontent, "OVPN_ACCESS_SERVER", &limits);
    validate_profile();

    signals_->LogInfo("Parsed"s + (persistent ? " persistent" : "")
                      + (persistent && single_use ? "," : "")
                      + (single_use ? " single-use" : "")
                      + " configuration '" + name
                      + ", owner: " + lookup_username(owner));

    if (persistent && !state_dir_.empty())
    {
        persistent_file_ = state_dir_ + "/" + simple_basename(GetPath()) + ".json";
        update_persistent_file();
    }

    add_methods();
    add_properties();
}


Configuration::Configuration(DBus::Connection::Ptr dbuscon,
                             DBus::Object::Manager::Ptr object_manager,
                             DBus::Credentials::Query::Ptr creds_qry,
                             ::Signals::ConfigurationManagerEvent::Ptr sig_configmgr,
                             const std::string &filename,
                             Json::Value profile,
                             uint8_t loglevel,
                             LogWriter::Ptr logwr)
    : DBus::Object::Base(profile["object_path"].asString(), INTERFACE_CONFIGMGR),
      object_manager_(object_manager), creds_qry_(creds_qry),
      sig_configmgr_(sig_configmgr), persistent_file_(filename)
{
    prop_persistent_ = !persistent_file_.empty();

    DisableIdleDetector(prop_persistent_);

    signals_ = ConfigManager::Log::Create(dbuscon,
                                          LogGroup::CONFIGMGR,
                                          GetPath(),
                                          logwr);
    signals_->SetLogLevel(loglevel);

    // Prepare the object handling access control lists
    object_acl_ = GDBusPP::Object::Extension::ACL::Create(dbuscon,
                                                          profile["owner"].asUInt64());

    prop_name_ = filter_ctrl_chars(profile["name"].asString(), true);
    prop_import_timestamp_ = profile["import_timestamp"].asUInt64();
    prop_last_used_timestamp_ = profile["last_used_timestamp"].asUInt64();
    prop_locked_down_ = profile["locked_down"].asBool();
    prop_transfer_owner_session_ = profile["transfer_owner_session"].asBool();
    prop_readonly_ = profile["readonly"].asBool();
    prop_single_use_ = profile["single_use"].asBool();
    prop_used_count_ = profile["used_count"].asUInt();
    prop_dco_ = profile["dco"].asBool();
    prop_valid_ = profile["valid"].asBool();

    if (profile["tags"].isArray())
    {
        for (const auto &tag : profile["tags"])
        {
            prop_tags_.push_back(filter_ctrl_chars(tag.asString(), true));
        }
    }

    object_acl_->SetPublicAccess(profile["public_access"].asBool());

    for (const auto &id : profile["acl"])
    {
        object_acl_->GrantAccess(id.asUInt());
    }

    for (const auto &ovkey : profile["overrides"].getMemberNames())
    {
        Json::Value ov = profile["overrides"][ovkey];
        switch (ov.type())
        {
        case Json::ValueType::booleanValue:
            try
            {
                set_override(ovkey, ov.asBool());
            }
            catch (const DBus::Object::Method::Exception &excp)
            {
                signals_->LogError("Incorrect override key: " + ovkey + ", ignoring");
            }
            break;

        case Json::ValueType::stringValue:
            try
            {
                set_override(ovkey, filter_ctrl_chars(ov.asString(), true));
            }
            catch (const DBus::Object::Method::Exception &excp)
            {
                signals_->LogError("Incorrect override key: " + ovkey + ", ignoring");
            }
            break;

        default:
            throw DBus::Object::Method::Exception("Invalid data type for ...");
        }
    }

    // Parse the options from the imported configuration
    options_.json_import(profile["profile"]);
    validate_profile();

    add_methods();
    add_properties();
}


const bool Configuration::Authorize(const DBus::Authz::Request::Ptr authzreq)
{
    switch (authzreq->operation)
    {
    case DBus::Object::Operation::METHOD_CALL:
        {
            // These methods are always restricted to the owner only;
            // we don't provide sharing user admin rights to configurations
            if (write_methods_.end() != write_methods_.find(authzreq->target))
            {
                return !prop_readonly_ && object_acl_->CheckOwnerAccess(authzreq->caller);
            }

            if ("net.openvpn.v3.configuration.Fetch" == authzreq->target
                || "net.openvpn.v3.configuration.FetchJSON" == authzreq->target)
            {
                if (prop_locked_down_)
                {
                    // If the configuration is locked down, restrict any
                    // read-operations to anyone except the backend VPN client
                    // process (openvpn user) or the configuration profile
                    // owner.
                    return object_acl_->CheckOwnerAccess(authzreq->caller)
                           || creds_qry_->GetUID(authzreq->caller) == lookup_uid(OPENVPN_USERNAME);
                }

                // We don't grant access to Fetch/FetchJSON with only public-access
                return object_acl_->CheckACL(authzreq->caller,
                                             {object_acl_->GetOwner(),
                                              lookup_uid(OPENVPN_USERNAME)},
                                             true);
            }

            // Only the owner is allowed to seal and remove configuration profiles
            static const std::set<std::string> owner_methods = {
                "net.openvpn.v3.configuration.Seal",
                "net.openvpn.v3.configuration.Remove"};
            if (owner_methods.end() != owner_methods.find(authzreq->target))
            {
                return object_acl_->CheckOwnerAccess(authzreq->caller);
            }

            return object_acl_->CheckACL(authzreq->caller,
                                         {object_acl_->GetOwner()});
        }

    case DBus::Object::Operation::PROPERTY_GET:
        {
            if ("net.openvpn.v3.configuration.owner" == authzreq->target)
                return true;

            // Owner + openvpn - optionally public access.
            static const std::set<std::string> props = {
                "net.openvpn.v3.configuration.name",
                "net.openvpn.v3.configuration.transfer_owner_session",
                "net.openvpn.v3.configuration.overrides",
                "net.openvpn.v3.configuration.dco",
                "net.openvpn.v3.configuration.persistent",
                "net.openvpn.v3.configuration.import_timestamp",
                "net.openvpn.v3.configuration.last_used_timestamp",
                "net.openvpn.v3.configuration.locked_down",
                "net.openvpn.v3.configuration.readonly",
                "net.openvpn.v3.configuration.used_count",
                "net.openvpn.v3.configuration.valid",
                "net.openvpn.v3.configuration.tags"};
            if (props.end() != props.find(authzreq->target))
            {
                return object_acl_->CheckACL(authzreq->caller,
                                             {object_acl_->GetOwner(),
                                              lookup_uid(OPENVPN_USERNAME)});
            }

            // Owner only.
            static const std::set<std::string> owner_props = {
                "net.openvpn.v3.configuration.public_access",
                "net.openvpn.v3.configuration.acl",
                "net.openvpn.v3.configuration.single_use"};
            if (owner_props.end() != owner_props.find(authzreq->target))
            {
                return object_acl_->CheckACL(authzreq->caller,
                                             {object_acl_->GetOwner()});
            }

            // Grant owner, granted users, root and the openvpn user access
            // to all other properties.
            return object_acl_->CheckACL(authzreq->caller,
                                         {0,
                                          object_acl_->GetOwner(),
                                          lookup_uid(OPENVPN_USERNAME)},
                                         true);
        }

    case DBus::Object::Operation::PROPERTY_SET:
        {
            if (prop_readonly_)
                return false;

            static const std::set<std::string> props = {
                "net.openvpn.v3.configuration.name",
                "net.openvpn.v3.configuration.locked_down",
                "net.openvpn.v3.configuration.public_access"};
            if (props.end() != props.find(authzreq->target))
            {
                return object_acl_->CheckOwnerAccess(authzreq->caller);
            }

            return object_acl_->CheckACL(authzreq->caller,
                                         {object_acl_->GetOwner()},
                                         true);
        }

    case DBus::Object::Operation::NONE:
    default:
        break;
    }

    return false;
}


const std::string Configuration::AuthorizationRejected(const DBus::Authz::Request::Ptr request) const noexcept
{
    switch (request->operation)
    {
    case DBus::Object::Operation::PROPERTY_SET:
        return (prop_readonly_
                    ? "Configuration profile is sealed and read-only"
                    : "Configuration can only be modified by the profile owner");

    case DBus::Object::Operation::METHOD_CALL:
        // If it's a read-only object and these methods are called,
        // we reject it with a better error message.
        if (prop_readonly_ && write_methods_.end() != write_methods_.find(request->target))
        {
            return "Configuration profile is sealed and read-only";
        }

    case DBus::Object::Operation::PROPERTY_GET:
    default:
        return "Access to the configuration profile is denied.";
    }
}


void Configuration::add_methods()
{
    AddMethod(
        "Validate",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            std::string validation = validate_profile();
            if (!validation.empty())
            {
                throw DBus::Object::Method::Exception(validation);
            }
            args->SetMethodReturn(nullptr);
        });

    auto fetch_args = AddMethod(
        "Fetch",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            method_fetch(args, false);

            bool is_backend_client = false;

            try
            {
                // There are two checks happening here:
                //
                // 1. Retrieve the PID value of the sender and the
                //    well-known bus name for the backend client
                //
                //    It is expected that the unqiue bus ID is
                //    tied to the same process as the well-known bus
                //    name, which indicates the correct information has
                //    been retrieved.
                //
                // 2. The well-known busname for the backend
                //    client is re-composed and the unique bus ID for
                //    this busname is retrieved from the main D-Bus service
                //
                //    This value will be used to compare the the unique
                //    bus ID with the bus ID provided in the "sender"
                //    variable.
                //
                // If both of these checks matches, the check is complete:
                // The PID of both the well-known and unique bus IDs
                // indicates it is the same process.  And the unique bus ID
                // from the well-known bus name (recomposed from the PID)
                // matches the unique bus ID from the requestor for this
                // call.

                const std::string caller = args->GetCallerBusName();
                pid_t caller_pid = creds_qry_->GetPID(caller);

                // Re-compose the well-known bus name from the PID of the
                // sender.  If this call comes from a PID not being a
                // backend client, it will not be able to retrieve any
                // unique bus ID for the sender.
                std::string be_name = SERVICE_BACKEND + std::to_string(caller_pid);
                std::string be_unique = creds_qry_->GetUniqueBusName(be_name);

                pid_t be_pid = creds_qry_->GetPID(be_name);

                // Check if everything matches
                is_backend_client = (caller_pid == be_pid) && (caller == be_unique);
            }
            catch (const DBus::Exception &e)
            {
                // If any of these D-Bus checks (GetPID/GetUniqueBusID)
                // fails, it is not a backend client service.

                is_backend_client = false;
            }

            if (is_backend_client)
            {
                // If this config is tagged as single-use only then we delete this
                // config from memory.
                if (prop_single_use_)
                {
                    signals_->LogVerb2("Single-use configuration fetched");
                    method_remove();
                    return;
                }

                prop_used_count_++;
                prop_last_used_timestamp_ = std::time(nullptr);
                update_persistent_file();
            }
        });

    fetch_args->AddOutput("config", glib2::DataType::DBus<std::string>());

    auto fj_args = AddMethod("FetchJSON",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_fetch(args, true);
                             });

    fj_args->AddOutput("config_json", glib2::DataType::DBus<std::string>());

    auto so_args = AddMethod("SetOption",
                             [](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 GVariant *params = args->GetMethodParameters();

                                 auto option = glib2::Value::Extract<std::string>(params, 0);
                                 auto value = glib2::Value::Extract<std::string>(params, 1);
                             });

    so_args->AddInput("option", glib2::DataType::DBus<std::string>());
    so_args->AddInput("value", glib2::DataType::DBus<std::string>());

    auto sov_args = AddMethod("SetOverride",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  method_set_override(args);
                              });

    sov_args->AddInput("name", glib2::DataType::DBus<std::string>());
    sov_args->AddInput("value", "v");

    auto uo_args = AddMethod("UnsetOverride",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_unset_override(args);
                             });

    uo_args->AddInput("name", glib2::DataType::DBus<std::string>());

    auto at_args = AddMethod("AddTag",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_add_tag(args);
                             });

    at_args->AddInput("tag", glib2::DataType::DBus<std::string>());

    auto rt_args = AddMethod("RemoveTag",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_remove_tag(args);
                             });

    rt_args->AddInput("tag", glib2::DataType::DBus<std::string>());

    auto ag_args = AddMethod("AccessGrant",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_access_grant(args);
                             });

    ag_args->AddInput("uid", glib2::DataType::DBus<uint32_t>());

    auto ar_args = AddMethod("AccessRevoke",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 method_access_revoke(args);
                             });

    ar_args->AddInput("uid", glib2::DataType::DBus<uint32_t>());

    AddMethod("Seal",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  method_seal();
              });

    AddMethod("Remove",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  method_remove();
              });
}


void Configuration::add_properties()
{
    AddProperty("persistent", prop_persistent_, false);
    AddProperty("import_timestamp", prop_import_timestamp_, false, "t");
    AddProperty("last_used_timestamp", prop_last_used_timestamp_, false, "t");
    AddProperty("readonly", prop_readonly_, false);
    AddProperty("single_use", prop_single_use_, false);
    AddProperty("tags", prop_tags_, false);
    AddProperty("used_count", prop_used_count_, false);
    AddProperty("valid", prop_valid_, false);

    AddPropertyBySpec(
        "owner",
        glib2::DataType::DBus<uint32_t>(),
        [this](const DBus::Object::Property::BySpec &prop)
        {
            return glib2::Value::Create(object_acl_->GetOwner());
        });

    AddPropertyBySpec("acl",
                      "au",
                      [this](const DBus::Object::Property::BySpec &prop)
                      {
                          using namespace glib2::Builder;

                          return Finish(FromVector(object_acl_->GetAccessList()));
                      });

    AddPropertyBySpec("overrides",
                      "a{sv}",
                      [this](const DBus::Object::Property::BySpec &prop)
                      {
                          GVariantBuilder *b = glib2::Builder::Create("a{sv}");

                          for (const auto &o : override_list_)
                          {
                              auto value = (std::holds_alternative<std::string>(o.value)
                                                ? glib2::Value::Create(std::get<std::string>(o.value))
                                                : glib2::Value::Create(std::get<bool>(o.value)));

                              g_variant_builder_add(b, "{sv}", o.key.c_str(), value);
                          }

                          return glib2::Builder::Finish(b);
                      });

    // Read-write properties need to also update the persistent disk file.

    AddPropertyBySpec(
        "public_access",
        "b",
        [this](const DBus::Object::Property::BySpec &prop)
        {
            return glib2::Value::Create(object_acl_->GetPublicAccess());
        },
        [this](const DBus::Object::Property::BySpec &prop, GVariant *value)
        {
            auto access = glib2::Value::Get<bool>(value);

            object_acl_->SetPublicAccess(access);

            auto upd = prop.PrepareUpdate();
            upd->AddValue(access);

            update_persistent_file();

            return upd;
        });

    add_persistent_property("name", "s", prop_name_);
    add_persistent_property("transfer_owner_session", "b", prop_transfer_owner_session_);
    add_persistent_property("locked_down", "b", prop_locked_down_);
    add_persistent_property("dco", "b", prop_dco_);
}


/**
 *  Exports the configuration, including all the available settings
 *  specific to the Linux client.  The output format is JSON.
 *
 * @return Returns a Json::Value object containing the serialized
 *         configuration profile
 */
Json::Value Configuration::Export() const
{
    Json::Value ret;

    ret["object_path"] = GetPath();
    ret["owner"] = (uint32_t)object_acl_->GetOwner();
    ret["name"] = filter_ctrl_chars(prop_name_, true);

    if (prop_tags_.size() > 0)
    {
        for (const auto &tag : prop_tags_)
        {
            ret["tags"].append(filter_ctrl_chars(tag, true));
        }
    }

    ret["import_timestamp"] = (Json::Value::UInt64)prop_import_timestamp_;
    ret["last_used_timestamp"] = (Json::Value::UInt64)prop_last_used_timestamp_;
    ret["locked_down"] = prop_locked_down_;
    ret["transfer_owner_session"] = prop_transfer_owner_session_;
    ret["readonly"] = prop_readonly_;
    ret["single_use"] = prop_single_use_;
    ret["used_count"] = prop_used_count_;
    ret["valid"] = prop_valid_;
    ret["profile"] = options_.json_export();
    ret["dco"] = prop_dco_;

    ret["public_access"] = object_acl_->GetPublicAccess();

    for (const auto &e : object_acl_->GetAccessList())
    {
        ret["acl"].append(e);
    }

    for (const auto &ov : override_list_)
    {
        if (std::holds_alternative<bool>(ov.value))
        {
            ret["overrides"][ov.key] = std::get<bool>(ov.value);
        }
        else
        {
            ret["overrides"][ov.key] = std::get<std::string>(ov.value);
        }
    }

    return ret;
}


void Configuration::TransferOwnership(uid_t new_owner_uid)
{
    object_acl_->TransferOwnership(new_owner_uid);
}


bool Configuration::CheckACL(const std::string &caller) const noexcept
{
    return object_acl_->CheckACL(caller, {object_acl_->GetOwner()});
}


void Configuration::update_persistent_file()
{
    if (persistent_file_.empty())
    {
        // If this configuration is not configured as persistent,
        // we're done.
        return;
    }

    std::ofstream state(persistent_file_);
    state << Export();

    signals_->LogVerb2("Updated persistent config: " + persistent_file_);
}


std::string Configuration::validate_profile() noexcept
{
    bool client_configured = false;
    bool remote_found = false;
    bool ca_found = false;
    bool dev_found = false;
    for (const auto &opt : options_)
    {
        try
        {
            if ("setenv" == opt.get(0, 32))
            {
                if ("GENERIC_CONFIG" == opt.get(1, 32))
                {
                    prop_valid_ = false;
                    return "Server locked profiles are unsupported";
                }
            }
            else if ("remote" == opt.get(0, 32))
            {
                remote_found = true;
            }
            else if ("ca" == opt.get(0, 32))
            {
                ca_found = true;
            }
            else if ("dev" == opt.get(0, 32))
            {
                dev_found = true;
            }
            else if ("client" == opt.get(0, 32)
                     || "tls-client" == opt.get(0, 32))
            {
                client_configured = true;
            }
        }
        catch (const std::exception &excp)
        {
            signals_->LogError("Failed validating profile: "
                               + std::string(excp.what()));
        }
    }
    if (!client_configured || !dev_found || !remote_found || !ca_found)
    {
        prop_valid_ = false;
        std::vector<std::string> opts_missing;
        if (!ca_found)
        {
            opts_missing.push_back("--ca");
        }
        if (!client_configured)
        {
            opts_missing.push_back("--client or --tls-client");
        }
        if (!dev_found)
        {
            opts_missing.push_back("--dev");
        }
        if (!remote_found)
        {
            opts_missing.push_back("--remote");
        }

        return fmt::format("Configuration profile is missing required options: {}",
                           fmt::join(opts_missing, ", "));
    }
    prop_valid_ = true;
    return "";
}


void Configuration::method_fetch(DBus::Object::Method::Arguments::Ptr args, bool json)
{
    std::stringstream config;

    if (json)
    {
        config << options_.json_export();
    }
    else
    {
        config << options_.string_export();
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(config.str()));
}


void Configuration::method_add_tag(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto tag = filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 0), true);

    if (tag.empty())
    {
        throw DBus::Object::Method::Exception("Tag value cannot be empty");
    }

    if (tag.length() > 128)
    {
        throw DBus::Object::Method::Exception("Tag value too long");
    }

    if (CheckForTag(tag))
    {
        throw DBus::Object::Method::Exception("Tag already exists");
    }

    prop_tags_.push_back(tag);
    update_persistent_file();
}


void Configuration::method_remove_tag(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto tag = glib2::Value::Extract<std::string>(params, 0);

    auto it = std::find(prop_tags_.begin(), prop_tags_.end(), tag);

    if (tag.empty())
    {
        throw DBus::Object::Method::Exception("Tag value cannot be empty");
    }

    if (tag.length() > 128)
    {
        throw DBus::Object::Method::Exception("Tag value too long");
    }

    if (it == prop_tags_.end())
    {
        throw DBus::Object::Method::Exception("Non-existing tag");
    }

    prop_tags_.erase(it);
    update_persistent_file();
}


void Configuration::method_set_override(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto name = filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 0), true);
    GVariant *value = g_variant_get_variant(g_variant_get_child_value(params, 1));

    const Override o = set_override(name, value);

    std::string new_value;

    if (std::holds_alternative<std::string>(o.value))
    {
        new_value = filter_ctrl_chars(std::get<std::string>(o.value), true);
    }
    else
    {
        new_value = std::get<bool>(o.value) ? "true" : "false";
    }

    const std::string caller = args->GetCallerBusName();

    signals_->LogInfo("Setting configuration override '" + name
                      + "' to '" + new_value + "' by UID "
                      + std::to_string(creds_qry_->GetUID(caller)));

    update_persistent_file();
}

void Configuration::method_unset_override(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto name = filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 0), true);

    if (remove_override(name))
    {
        const std::string caller = args->GetCallerBusName();

        signals_->LogInfo("Unset configuration override '" + name + "' by UID "
                          + std::to_string(creds_qry_->GetUID(caller)));
    }
    else
    {
        throw DBus::Object::Method::Exception("Override '" + name + "' has not been set");
    }

    update_persistent_file();
}


void Configuration::method_access_grant(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    uid_t uid = glib2::Value::Extract<uid_t>(params, 0);

    try
    {
        object_acl_->GrantAccess(uid);
    }
    catch (const GDBusPP::Object::Extension::ACLException &excp)
    {
        throw DBus::Object::Method::Exception(excp.GetRawError());
    }

    signals_->LogInfo("Granted access to " + lookup_username(uid) + " on " + GetPath());

    update_persistent_file();
}


void Configuration::method_access_revoke(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    uid_t uid = glib2::Value::Extract<uid_t>(params, 0);

    try
    {
        object_acl_->RevokeAccess(uid);
    }
    catch (const GDBusPP::Object::Extension::ACLException &excp)
    {
        throw DBus::Object::Method::Exception(excp.GetRawError());
    }

    signals_->LogInfo("Revoked access from " + lookup_username(uid) + " on " + GetPath());

    update_persistent_file();
}


void Configuration::method_seal()
{
    if (!prop_valid_)
        throw DBus::Object::Method::Exception("Configuration is not currently valid");

    prop_readonly_ = true;
    update_persistent_file();
}


void Configuration::method_remove()
{
    object_manager_->RemoveObject(GetPath());

    if (!persistent_file_.empty())
    {
        std::remove(persistent_file_.c_str());
    }

    sig_configmgr_->Send(GetPath(),
                         EventType::CFG_DESTROYED,
                         object_acl_->GetOwner());
}


Override Configuration::set_override(const std::string &key, GVariant *value)
{
    auto o = GetConfigOverride(key);

    if (!o)
    {
        throw DBus::Object::Method::Exception("Invalid override key '"
                                              + std::string(key) + "'");
    }

    // Ensure that a previous override value is removed
    remove_override(key);

    std::string g_type = glib2::DataType::Extract(value);

    if ("s" == g_type)
    {
        if (!std::holds_alternative<std::string>(o->value))
        {
            throw DBus::Object::Method::Exception("(SetOverride) Invalid override data type for '"
                                                  + key + "': " + g_type);
        }

        std::string v = filter_ctrl_chars(glib2::Value::Get<std::string>(value), true);
        return set_override(key, v);
    }
    else if ("b" == g_type)
    {
        if (!std::holds_alternative<bool>(o->value))
        {
            throw DBus::Object::Method::Exception("(SetOverride) Invalid override data type for '"
                                                  + key + "': " + g_type);
        }
        return set_override(key, glib2::Value::Get<bool>(value));
    }

    throw DBus::Object::Method::Exception("Unsupported override data type: " + g_type);
}


bool Configuration::remove_override(const std::string &key)
{
    for (auto it = override_list_.begin(); it != override_list_.end(); it++)
    {
        if (it->key == key)
        {
            override_list_.erase(it);
            return true;
        }
    }

    return false;
}

} // namespace ConfigManager
