//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020-  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file   config/config-manage.hpp
 *
 * @brief  Implementation of all the various openvpn3 config-* commands
 */

#include "build-config.h"
#include <json/json.h>
#include <gdbuspp/connection.hpp>

#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../../arghelpers.hpp"


namespace ovpn3cli::config::manage {

class ConfigProfileDetails
{
  public:
    using Ptr = std::shared_ptr<ConfigProfileDetails>;

    [[nodiscard]] static Ptr Create(OpenVPN3ConfigurationProxy::Ptr cfgprx);

    bool AccessAllowed() const noexcept;

    DBus::Object::Path path{};
    std::string name = "(n/a)";
    std::string tags = "(n/a)";
    std::string dns_scope = "(n/a)";
    std::string sealed = "(n/a)";
    std::string persistent = "(n/a)";
    std::string dco = "(n/a)";
    std::map<std::string, std::string> overrides{};
    std::string invalid_reason = "";

  private:
    OpenVPN3ConfigurationProxy::Ptr prx = nullptr;
    bool access_allowed = false;

    ConfigProfileDetails(OpenVPN3ConfigurationProxy::Ptr cfgprx);

    std::string extract_tags() const;

    std::string extract_override(const std::string &override,
                                 const std::string &unavail_value,
                                 const std::string &bool_true = "true",
                                 const std::string &bool_false = "false") const;
    void parse_overrides();
};


ConfigProfileDetails::Ptr ConfigProfileDetails::Create(OpenVPN3ConfigurationProxy::Ptr p)
{
    return ConfigProfileDetails::Ptr(new ConfigProfileDetails(p));
}


ConfigProfileDetails::ConfigProfileDetails(OpenVPN3ConfigurationProxy::Ptr cfgprx)
    : prx(cfgprx)
{
    try
    {
        path = prx->GetConfigPath();
        name = prx->GetName();
        tags = extract_tags();
        dns_scope = extract_override("dns-scope", "global (default)");
        sealed = prx->GetSealed() ? "Yes" : "No";
        persistent = prx->GetPersistent() ? "Yes" : "No";
        dco = prx->GetDCO() ? "Yes" : "No";
        parse_overrides();
        access_allowed = true;

        try
        {
            prx->Validate();
        }
        catch (const CfgMgrProxyException &excp)
        {
            invalid_reason = std::string(excp.GetRawError());
        }
    }
    catch (const DBus::Exception &excp)
    {
        access_allowed = false;
    }
}


bool ConfigProfileDetails::AccessAllowed() const noexcept
{
    return access_allowed;
}

std::string ConfigProfileDetails::extract_tags() const
{
    std::string tags = "";
    try
    {
        if (prx->CheckFeatures(CfgMgrFeatures::TAGS))
        {
            bool first = true;
            for (const auto &t : prx->GetTags())
            {
                tags += (!first ? ", " : "");
                tags += t;
                first = false;
            }
        }
    }
    catch (const DBus::Exception &ex)
    {
        tags = "(not available)";
    }
    return tags;
}

std::string ConfigProfileDetails::extract_override(const std::string &override,
                                                   const std::string &unavail_value,
                                                   const std::string &bool_true,
                                                   const std::string &bool_false) const
{
    try
    {
        const OverrideValue &ov = prx->GetOverrideValue(override);
        switch (ov.override.type)
        {
        case OverrideType::string:
            return ov.strValue;

        case OverrideType::boolean:
            return (ov.boolValue ? bool_true : bool_false);

        case OverrideType::invalid:
        default:
            return "(invalid)";
        }
    }
    catch (const DBus::Exception &ex)
    {
        return unavail_value;
    }
    return "(inaccessible)";
}


void ConfigProfileDetails::parse_overrides()
{
    for (const auto &vo : configProfileOverrides)
    {
        std::string value = "(not set)";
        for (const auto &ov : prx->GetOverrides(false))
        {
            if ("dns-scope" == ov.override.key)
            {
                // This override is retrieved in the global block
                continue;
            }
            if (ov.override.key == vo.key)
            {
                switch (ov.override.type)
                {
                case OverrideType::string:
                    overrides[vo.key] = ov.strValue;
                    break;

                case OverrideType::boolean:
                    overrides[vo.key] = ov.boolValue ? "true" : "false";
                    break;

                case OverrideType::invalid:
                    overrides[vo.key] = "(invalid)";
                    break;
                }
            }
        }
    }
}


} // namespace ovpn3cli::config::manage

/**
 * openvpn3 config-manage --show command
 *
 *  This command shows all overrides and other details about the config
 *  apart from the config itself
 */
static int config_manage_show(OpenVPN3ConfigurationProxy::Ptr conf)
{

    try
    {
        auto prf = ovpn3cli::config::manage::ConfigProfileDetails::Create(conf);

        if (!prf->AccessAllowed())
        {
            throw CommandException("config-manage", "No access to profile");
        }

        // Right algin the field with explicit width
        std::cout << std::endl
                  << std::right << std::setw(32)
                  << "    Configuration path: " << prf->path << std::endl
                  << "                          Name: " << prf->name << std::endl;

        if (!prf->tags.empty())
        {
            std::cout << std::setw(32) << "                  Tags: "
                      << prf->tags << std::endl;
        }
        std::cout << std::setw(32) << "             Read only: "
                  << prf->sealed << std::endl
                  << std::setw(32) << "     Persistent config: "
                  << prf->persistent << std::endl;
#ifdef ENABLE_OVPNDCO
        std::cout << std::setw(32) << "  Data Channel Offload: "
                  << prf->dco << std::endl;
#endif

        std::cout << std::setw(32) << "    DNS Resolver Scope: "
                  << prf->dns_scope << std::endl;

        if (!prf->invalid_reason.empty())
        {
            std::cout << std::endl
                      << "    *WARNING*  Invalid profile:"
                      << prf->invalid_reason
                      << std::endl;
        }

        std::cout << std::endl
                  << "  Overrides: ";
        if (prf->overrides.empty())
        {
            std::cout << " No overrides set." << std::endl;
        }
        else
        {
            std::cout << std::endl;
            for (const auto &[key, value] : prf->overrides)
            {
                std::cout << std::setw(30) << key
                          << ": " << value << std::endl;
            }
        }
        std::cout << std::endl;
        return 0;
    }
    catch (const DBus::Exception &err)
    {
        throw CommandException("config-manage --show", err.GetRawError());
    }
}

/**
 *  openvpn3 config-manage command
 *
 *  Manages configuration profile properties.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_manage(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-manage", "No configuration provided "
                                                "(--path, --config)");
    }

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);

    std::string path = (args->Present("config")
                            ? retrieve_config_path("config-manage",
                                                   args->GetValue("config", 0),
                                                   dbuscon)
                            : args->GetValue("path", 0));

    auto conf = OpenVPN3ConfigurationProxy::Create(dbuscon, path);
    if (!conf->CheckObjectExists())
    {
        throw CommandException("config-manage",
                               "Configuration profile does not exist");
    }

    bool quiet = args->Present("quiet");
    if (args->Present("exists"))
    {
        try
        {
            if (!quiet)
            {
                std::cout << "Configuration '" << conf->GetName()
                          << "' is present" << std::endl;
            }
            return 0;
        }
        catch (const CommandException &excp)
        {
            if (!quiet)
            {
                std::cout << excp.what() << std::endl;
            }
            return 1;
        }
    }


    bool override_present = false;
    for (const ValidOverride &vo : configProfileOverrides)
    {
        if (args->Present(vo.key)
            && (args->Present("unset-override") && args->GetValue("unset-override", 0) == vo.key))
        {
            throw CommandException("config-manage",
                                   "Cannot provide both --" + vo.key + " and "
                                       + "--unset-" + vo.key
                                       + " at the same time.");
        }
        if (args->Present(vo.key))
        {
            override_present = true;
        }
    }

    if ((args->Present({"rename", "show", "tag", "remove-tag"}, true).empty())
#ifdef ENABLE_OVPNDCO
        && !args->Present("dco")
#endif
        && !override_present && !args->Present("unset-override"))
    {
        throw CommandException("config-manage",
                               "An operation argument is required "
                               "(--rename, --show, --tag, --remove-tag"
                               "--<overrideName>, --unset-override"
#ifdef ENABLE_OVPNDCO
                               " or --dco"
#endif
                               ")");
    }

    try
    {
        bool valid_option = false;

        if (args->Present("rename"))
        {
            conf->SetName(args->GetValue("rename", 0));
            if (!quiet)
            {
                std::cout << "Configuration renamed" << std::endl;
            }
            valid_option = true;
        }

        if (!args->Present({"tag", "remove-tag"}, true).empty())
        {
            if (!conf->CheckFeatures(CfgMgrFeatures::TAGS))
            {
                throw CommandException("config-manage",
                                       "The currently running configuration manager does not support tags");
            }
        }

        if (args->Present("remove-tag"))
        {
            valid_option = true;
            std::string taglist;
            for (unsigned int i = 0; i < args->GetValueLen("remove-tag"); i++)
            {
                try
                {
                    conf->RemoveTag(args->GetValue("remove-tag", i));
                    taglist = taglist
                              + (taglist.empty() ? "" : ", ")
                              + args->GetValue("remove-tag", i);
                }
                catch (const CfgMgrProxyException &err)
                {
                    std::cerr << "Warning: " << err.GetRawError() << std::endl;
                }
            }
            if (!taglist.empty() && !quiet)
            {
                std::cout << "Removed tag"
                          << (args->GetValueLen("tag") != 1 ? "s" : "") << ": "
                          << taglist << std::endl;
            }
        }

        if (args->Present("tag"))
        {
            valid_option = true;
            std::string taglist;
            for (unsigned int i = 0; i < args->GetValueLen("tag"); i++)
            {
                try
                {
                    conf->AddTag(args->GetValue("tag", i));
                    taglist = taglist
                              + (taglist.empty() ? "" : ", ")
                              + args->GetValue("tag", i);
                }
                catch (const CfgMgrProxyException &err)
                {
                    std::cerr << "Warning: " << err.GetRawError() << std::endl;
                }
            }
            if (!taglist.empty() && !quiet)
            {
                std::cout << "Added tag"
                          << (args->GetValueLen("tag") != 1 ? "s" : "") << ": "
                          << taglist << std::endl;
            }
        }


#ifdef ENABLE_OVPNDCO
        if (args->Present("dco"))
        {
            bool dco = args->GetBoolValue("dco", false);
            conf->SetDCO(dco);

            if (!quiet)
            {
                std::cout << "Kernel based data channel offload support is "
                          << (dco ? "enabled" : "disabled") << std::endl;
            }
            valid_option = true;
        }
#endif

        for (const ValidOverride &vo : configProfileOverrides)
        {
            if (args->Present(vo.key))
            {
                if (OverrideType::boolean == vo.type)
                {
                    bool value = args->GetBoolValue(vo.key, 0);
                    conf->SetOverride(vo, value);
                    if (!quiet)
                    {
                        std::cout << "Override '" + vo.key + "' is " + (value ? "enabled" : "disabled")
                                  << std::endl;
                    }
                }
                else if (OverrideType::string == vo.type)
                {
                    std::string value = args->GetValue(vo.key, 0);
                    conf->SetOverride(vo, value);
                    if (!quiet)
                    {
                        std::cout << "Set override '" + vo.key + "' to '" + value + "'"
                                  << std::endl;
                    }
                }
                valid_option = true;
            }
        }

        if (args->Present("unset-override"))
        {
            for (const auto &key : args->GetAllValues("unset-override"))
            {
                const ValidOverride &override = GetConfigOverride(key, true);

                if (OverrideType::invalid == override.type)
                {
                    throw CommandException("config-manage",
                                           "Unsetting invalid override "
                                               + key + " is not possible");
                }

                try
                {
                    conf->UnsetOverride(override);
                    if (!quiet)
                    {
                        std::cout << "Unset override '" + override.key + "'" << std::endl;
                    }
                    valid_option = true;
                }
                catch (const DBus::Exception &err)
                {
                    std::string e(err.what());
                    if (e.find("net.openvpn.v3.error.OverrideNotSet") != std::string::npos)
                    {
                        if (!quiet)
                        {
                            std::cout << "Override '" << key
                                      << "' not set" << std::endl;
                        }
                    }
                    else
                    {
                        throw;
                    }
                }
            }
        }

        if (args->Present("show"))
        {
            if (valid_option)
            {
                std::cout << std::endl
                          << "------------------------------"
                          << "------------------------------"
                          << std::endl;
            }
            config_manage_show(conf);
            valid_option = true;
        }

        if (!valid_option)
        {
            throw CommandException("config-manage", "No operation option recognised");
        }
    }
    catch (DBus::Exception &err)
    {
        throw CommandException("config-manage", err.GetRawError());
    }
    catch (...)
    {
        throw;
    }
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'config-manage' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_manage()
{
    //
    //  config-manage command
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-manage",
                                "Manage configuration properties",
                                cmd_config_manage));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "CONFIG-PATH",
                                   true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("rename",
                   'r',
                   "NEW-CONFIG-NAME",
                   true,
                   "Renames the configuration");
    cmd->AddOption("tag",
                   "TAG-NAME",
                   true,
                   "Adds a tag name to the configuration profile");
    cmd->AddOption("remove-tag",
                   "TAG-NAME",
                   true,
                   "Removes a tag name to the configuration profile");
    cmd->AddOption("show",
                   's',
                   "Show current configuration options");
    cmd->AddOption("exists",
                   "Checks if a specific configuration file exists");
    cmd->AddOption("quiet",
                   "Don't write anything to terminal unless strictly needed");
#ifdef ENABLE_OVPNDCO
    cmd->AddOption("dco",
                   "<true|false>",
                   true,
                   "Set/unset the kernel data channel offload flag",
                   arghelper_boolean);
#endif

    // Generating options for all configuration profile overrides
    // as defined in overrides.hpp
    for (const auto &override : configProfileOverrides)
    {
        if (OverrideType::boolean == override.type)
        {
            cmd->AddOption(override.key,
                           "<true|false>",
                           true,
                           "Adds the boolean override " + override.key,
                           arghelper_boolean);
        }
        else
        {
            std::string help = "<value>";
            if (override.argument_helper)
            {
                help = "<" + override.argument_helper() + ">";
                std::replace(help.begin(), help.end(), ' ', '|');
            }
            cmd->AddOption(override.key, help, true, override.help, override.argument_helper);
        }
    }
    cmd->AddOption("unset-override", "<name>", true, "Removes the <name> override", arghelper_unset_overrides);

    return cmd;
}
