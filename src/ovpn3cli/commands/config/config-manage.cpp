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


/**
 * openvpn3 config-manage --show command
 *
 *  This command shows all overrides and other details about the config
 *  apart from the config itself
 */
static int config_manage_show(OpenVPN3ConfigurationProxy::Ptr conf,
                              bool showall = false)
{

    try
    {
        std::string dns_scope = "(not available)";
        try
        {
            const OverrideValue &ov = conf->GetOverrideValue("dns-scope");
            if (ov.override.valid())
            {
                dns_scope = ov.strValue;
            }
        }
        catch (const DBus::Exception &ex)
        {
            dns_scope = "global (default)";
        }

        std::string tags = "";
        try
        {
            if (conf->CheckFeatures(CfgMgrFeatures::TAGS))
            {
                bool first = true;
                for (const auto &t : conf->GetTags())
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


        // Right algin the field with explicit width
        std::cout << std::right;
        std::cout << std::setw(32) << "                  Name: "
                  << conf->GetName() << std::endl;
        if (!tags.empty())
        {
            std::cout << std::setw(32) << "                  Tags: "
                      << tags << std::endl;
        }
        std::cout << std::setw(32) << "             Read only: "
                  << (conf->GetSealed() ? "Yes" : "No") << std::endl
                  << std::setw(32) << "     Persistent config: "
                  << (conf->GetPersistent() ? "Yes" : "No") << std::endl;
#ifdef ENABLE_OVPNDCO
        std::cout << std::setw(32) << "  Data Channel Offload: "
                  << (conf->GetDCO() ? "Yes" : "No") << std::endl;
#endif

        std::cout << std::setw(32) << "    DNS Resolver Scope: "
                  << dns_scope << std::endl;

        std::cout << std::endl
                  << "  Overrides: ";
        auto overrides = conf->GetOverrides(false);
        if (overrides.empty() && !showall)
        {
            std::cout << " No overrides set." << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }

        for (auto &vo : configProfileOverrides)
        {
            std::string value = "(not set)";
            for (auto &ov : overrides)
            {
                if ("dns-scope" == ov.override.key)
                {
                    // This override is retrieved in the global block
                    continue;
                }
                if (ov.override.key == vo.key)
                {
                    if (OverrideType::boolean == ov.override.type)
                        value = ov.boolValue ? "true" : "false";
                    else
                        value = ov.strValue;
                }
            }
            if (showall || value != "(not set)")
            {
                std::cout << std::setw(30) << vo.key << ": " << value << std::endl;
            }
        }
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
                               "Configuration does not exist");
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
                    std::cerr << "Warning: " << err.what() << std::endl;
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
                    std::cerr << "Warning: " << err.what() << std::endl;
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
            std::cout << std::endl;
            config_manage_show(conf);
            std::cout << std::endl;
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
