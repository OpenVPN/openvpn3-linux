//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2021  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2021  David Sommerseth <davids@openvpn.net>
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
 * @file   config.hpp
 *
 * @brief  Implementation of all the various openvpn3 config-* commands
 */

#include "config.h"

#define USE_TUN_BUILDER
#include <client/ovpncli.cpp>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../arghelpers.hpp"

using namespace openvpn;

/**
 *  Parses and imports an OpenVPN configuration file and saves it
 *  within the OpenVPN 3 Configuration Manager service.
 *
 *  This parser will also ensure all external files are embedded into
 *  the configuration before sent to the configuration manager.
 *
 *  @param filename    Filename of the configuration file to import
 *  @param cfgname     Configuration name to be used inside the configuration manager
 *  @param single_use  This will make the Configuration Manager automatically
 *                     delete the configuration file from its storage upon the first
 *                     use.  This is used for load-and-connect scenarios where it
 *                     is not likely the configuration will be re-used
 *  @param persistent  This will make the Configuration Manager store the configuration
 *                     to disk, to be re-used later on.
 *
 *  @return Retuns a string containing the D-Bus object path to the configuration
 */
std::string import_config(const std::string filename,
                          const std::string cfgname,
                          const bool single_use,
                          const bool persistent)
{
    // Parse the OpenVPN configuration
    // The ProfileMerge will ensure that all needed
    // files are embedded into the configuration we
    // send to and store in the Configuration Manager
    ProfileMerge pm(filename, "", "",
                    ProfileMerge::FOLLOW_FULL,
                    ProfileParseLimits::MAX_LINE_SIZE,
                    ProfileParseLimits::MAX_PROFILE_SIZE);

    if (pm.status() != ProfileMerge::MERGE_SUCCESS)
    {
        throw CommandException("config-import", pm.error());
    }

    // Basic profile limits
    OptionList::Limits limits("profile is too large",
                              ProfileParseLimits::MAX_PROFILE_SIZE,
                              ProfileParseLimits::OPT_OVERHEAD,
                              ProfileParseLimits::TERM_OVERHEAD,
                              ProfileParseLimits::MAX_LINE_SIZE,
                              ProfileParseLimits::MAX_DIRECTIVE_SIZE);

    // Try to find persist-tun, which we will process further once imported
    bool persist_tun = false;
    try
    {
        OptionList opts;
        opts.parse_from_config(pm.profile_content(), &limits);
        opts.update_map();

        Option pt = opts.get("persist-tun");
        persist_tun = true;
    }
    catch (...)
    {
        // We ignore errors here - it means the option is not found
    }

    // Import the configuration fileh
    OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration);
    conf.Ping();
    std::string  cfgpath = conf.Import(cfgname, pm.profile_content(),
                                       single_use, persistent);

    // If the configuration profile contained --persist-tun,
    // set the related property in the D-Bus configuration object.
    //
    // The --persist-tun option in the configuration file is not processed
    // by the core OpenVPN 3 client itself, it needs to be set outside of
    // the configuration profile.  This is by design, mandated by
    // OpenVPN 3 Core library.
    if (persist_tun)
    {
        OpenVPN3ConfigurationProxy cfgprx(G_BUS_TYPE_SYSTEM, cfgpath);
        const ValidOverride& vo = cfgprx.LookupOverride("persist-tun");
        cfgprx.SetOverride(vo, true);
    }

    // Return the object path to this configuration profile
    return cfgpath;
}


/**
 *  openvpn3 config-import command
 *
 *  Imports a configuration file into the Configuration Manager.  This
 *  import operation will also embedd all external files into the imported
 *  profile as well.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_import(ParsedArgs::Ptr args)
{
    if (!args->Present("config"))
    {
        throw CommandException("config-import", "Missing required --config option");
    }
    try
    {
        std::string name = (args->Present("name") ? args->GetValue("name", 0)
                                                : args->GetValue("config", 0));
        std::string path = import_config(args->GetValue("config", 0),
                                         name,
                                         false,
                                         args->Present("persistent"));
        std::cout << "Configuration imported.  Configuration path: "
                  << path
                  << std::endl;
        return 0;
    }
    catch (std::exception& e)
    {
        throw CommandException("config-import", e.what());
    }
}


/**
 *  Creates the SingleCommand object for the 'config-import' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_import()
{
    //
    //  config-import command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-import",
                                "Import configuration profiles",
                                cmd_config_import));
    cmd->AddOption("config", 'c', "CFG-FILE", true,
                   "Configuration file to import");
    cmd->AddOption("name", 'n', "NAME", true,
                   "Provide a different name for the configuration (default: CFG-FILE)");
    cmd->AddOption("persistent", 'p',
                   "Make the configuration profile persistent through service restarts");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 config-list command
 *
 *  Lists all available configuration profiles.  Only profiles where the
 *  calling user is the owner, have been added to the access control list
 *  or profiles tagged with public_access will be listed.  This restriction
 *  is handled by the Configuration Manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_configs_list(ParsedArgs::Ptr args)
{
    OpenVPN3ConfigurationProxy confmgr(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration );
    confmgr.Ping();

    std::cout << "Configuration path" << std::endl;
    std::cout << "Imported" << std::setw(32-8) << std::setfill(' ') << " "
              << "Last used" << std::setw(26-9) << " "
              << "Used"
              << std::endl;
    std::cout << "Name" << std::setw(58-4) << " " << "Owner"
              << std::endl;
    std::cout << std::setw(32+26+18+2) << std::setfill('-') << "-" << std::endl;

    bool first = true;
    for (auto& cfg : confmgr.FetchAvailableConfigs())
    {
        if (cfg.empty())
        {
            continue;
        }
        OpenVPN3ConfigurationProxy cprx(G_BUS_TYPE_SYSTEM, cfg);

        if (!first)
        {
            std::cout << std::endl;
        }
        first = false;

        std::string name = cprx.GetStringProperty("name");
        std::string user = lookup_username(cprx.GetUIntProperty("owner"));

        std::time_t imp_tstamp = cprx.GetUInt64Property("import_timestamp");
        std::string imported(std::asctime(std::localtime(&imp_tstamp)));
        imported.erase(imported.find_last_not_of(" \n")+1); // rtrim

        std::time_t last_u_tstamp = cprx.GetUInt64Property("last_used_timestamp");
        std::string last_used;
        if (last_u_tstamp > 0)
        {
            last_used = std::asctime(std::localtime(&last_u_tstamp));
            last_used.erase(last_used.find_last_not_of(" \n")+1);  // rtrim
        }
        unsigned int used_count = cprx.GetUIntProperty("used_count");

        std::cout << cfg << std::endl;
        std::cout << imported << std::setw(32 - imported.size()) << std::setfill(' ') << " "
                  << last_used <<  std::setw(26 - last_used.size()) << " "
                  << std::to_string(used_count)
                  << std::endl;
        std::cout << name << std::setw(58 - name.size()) << " " << user
                  << std::endl;
    }
    std::cout << std::setw(32+26+18+2) << std::setfill('-') << "-" << std::endl;
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'config-list' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_configs_list()
{
    //
    //  config-list command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("configs-list",
                                "List all available configuration profiles",
                                cmd_configs_list));
    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 * openvpn3 config-manage --show command
 *
 *  This command shows all overrides and other details about the config
 *  apart from the config itself
 */
static int config_manage_show(OpenVPN3ConfigurationProxy& conf,
                                  bool showall = false)
{

    try
    {
        // Right algin the field with explicit width
        std::cout << std::right;
        std::cout << std::setw(32) << "                  Name: "
                  << conf.GetStringProperty("name") << std::endl
                  << std::setw(32) << "             Read only: "
                  << (conf.GetBoolProperty("readonly") ? "Yes" : "No") << std::endl
                  << std::setw(32) << "     Persistent config: "
                  << (conf.GetBoolProperty("persistent") ? "Yes" : "No") << std::endl;
#ifdef ENABLE_OVPNDCO
        std::cout << std::setw(32) << "  Data Channel Offload: "
                  << (conf.GetDCO() ? "Yes" : "No") << std::endl;
#endif

        std::cout << std::endl << "  Overrides: ";
        auto overrides = conf.GetOverrides();
        if (overrides.empty() && !showall)
        {
            std::cout << " No overrides set." << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }

        for (auto & vo: configProfileOverrides)
        {
            std::string value = "(not set)";
            for (auto & ov: overrides)
            {
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
    catch (DBusException& err)
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
    bool override_present = false;
    for (const ValidOverride& vo : configProfileOverrides)
    {
        if (args->Present(vo.key)
            && (args->Present("unset-override") && args->GetValue("unset-override", 0) == vo.key))
        {
            throw CommandException("config-manage",
                                   "Cannot provide both --" + vo.key +" and "
                                   + "--unset-" + vo.key
                                   + " at the same time.");
        }
        if (args->Present(vo.key))
        {
            override_present = true;
        }
    }

    if (!args->Present("rename") && !args->Present("show")
#ifdef ENABLE_OVPNDCO
        && !args->Present("dco")
#endif
        && !override_present && !args->Present("unset-override"))
    {
        throw CommandException("config-manage",
                               "An operation argument is required "
                               "(--rename, --show, "
                               "--<overrideName>, --unset-override"
#ifdef ENABLE_OVPNDCO
                               " or --dco"
#endif
                               ")"
                               );
    }

    try
    {

        std::string path = (args->Present("config")
                             ? retrieve_config_path("config-manage",
                                                    args->GetValue("config", 0))
                             : args->GetValue("path", 0));
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);
        if (!conf.CheckObjectExists())
        {
            throw CommandException("config-manage",
                                   "Configuration does not exist");
        }

        bool valid_option = false;

        if (args->Present("rename"))
        {
            conf.SetName(args->GetValue("rename", 0));
            std::cout << "Configuration renamed" << std::endl;
            valid_option = true;
        }

#ifdef ENABLE_OVPNDCO
        if (args->Present("dco"))
        {
            bool dco = args->GetBoolValue("dco", false);
            conf.SetDCO(dco);

            std::cout << "Kernel based data channel offload support is "
                      << (dco ? "enabled" : "disabled") << std::endl;
            valid_option = true;
        }
#endif

        for (const ValidOverride& vo: configProfileOverrides)
        {
            if (args->Present(vo.key))
            {
                if (OverrideType::boolean == vo.type)
                {
                    bool value = args->GetBoolValue(vo.key, 0);
                    conf.SetOverride(vo, value);
                    std::cout << "Override '" + vo.key + "' is " + (value ? "enabled" : "disabled")
                              << std::endl;
                }
                else if (OverrideType::string == vo.type)
                {
                    std::string value = args->GetValue(vo.key, 0);
                    conf.SetOverride(vo, value);
                    std::cout << "Set override '" + vo.key + "' to '" + value +"'"
                              << std::endl;
                }
                valid_option = true;
            }
        }

        if (args->Present("unset-override"))
        {
            for (const auto& key : args->GetAllValues("unset-override"))
            {
                const ValidOverride& override = GetConfigOverride(key, true);

                if (OverrideType::invalid == override.type)
                {
                    throw CommandException("config-manage",
                                           "Unsetting invalid override "
                                           + key + " is not possible");
                }

                try
                {
                    conf.UnsetOverride(override);
                    std::cout << "Unset override '" + override.key + "'" << std::endl;
                    valid_option = true;
                }
                catch (DBusException& err)
                {
                    std::string e(err.what());
                    if (e.find("net.openvpn.v3.error.OverrideNotSet") != std::string::npos)
                    {
                        std::cout << "Override '" << key
                                  << "' not set" << std::endl;
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
    catch (DBusPropertyException& err)
    {
        throw CommandException("config-manage", err.GetRawError());
    }
    catch (DBusException& err)
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
    auto path_opt = cmd->AddOption("path", 'o', "CONFIG-PATH", true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config", 'c', "CONFIG-NAME", true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("rename", 'r', "NEW-CONFIG-NAME", true,
                   "Renames the configuration");
    cmd->AddOption("show", 's',
                    "Show current configuration options");
#ifdef ENABLE_OVPNDCO
    cmd->AddOption("dco", "<true|false>", true,
                   "Set/unset the kernel data channel offload flag",
                   arghelper_boolean);
#endif

    // Generating options for all configuration profile overrides
    // as defined in overrides.hpp
    for (const auto& override : configProfileOverrides)
    {
        if (OverrideType::boolean == override.type)
        {
            cmd->AddOption(override.key, "<true|false>", true,
                           "Adds the boolean override " + override.key,
                           arghelper_boolean);
        }
        else
        {
            std::string help = "<value>";
            if (override.argument_helper)
            {
                help = "<" + override.argument_helper()  + ">";
                std::replace(help.begin(), help.end(), ' ', '|');
            }
            cmd->AddOption(override.key, help, true,
                           override.help,
                           override.argument_helper);
        }
    }
    cmd->AddOption("unset-override", "<name>", true,
                   "Removes the <name> override",
                   arghelper_unset_overrides);

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 config-acl command
 *
 *  Command to modify the access control to a specific configuration profile.
 *  All operations related to granting, revoking, public-access, locking-down
 *  and sealing (making configurations read-only) are handled by this command.
 *
 *  Also note that you can run multiple operarations in a single command line.
 *  It is fully possible to use --grant, --revoke and --show in a single
 *  command line.  In addition, both --grant and --revoke can be used multiple
 *  times to grant/revoke several users in a single operation.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_acl(ParsedArgs::Ptr args)
{
    int ret = 0;
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-acl",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        args->Present({"show", "grant", "revoke", "public-access", "lock-down",
                        "seal", "transfer-owner-session"});
    }
    catch (const OptionNotFound&)
    {
        throw CommandException("config-acl", "No operation option provided");
    }

    try
    {
        std::string path = (args->Present("config")
                             ? retrieve_config_path("config-acl",
                                                    args->GetValue("config", 0))
                             : args->GetValue("path", 0));
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);
        if (!conf.CheckObjectExists())
        {
            throw CommandException("config-acl",
                                   "Configuration does not exist");
        }

        if (args->Present("grant"))
        {
            for (auto const& user : args->GetAllValues("grant"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        conf.AccessGrant(uid);
                        std::cout << "Granted access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException& e)
                    {
                        std::cerr << "Failed granting access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException&)
                {
                    std::cerr << "** ERROR ** --grant " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }

        if (args->Present("revoke"))
        {
            for (auto const& user : args->GetAllValues("revoke"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        conf.AccessRevoke(uid);
                        std::cout << "Revoked access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException& e)
                    {
                        std::cerr << "Failed revoking access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException&)
                {
                    std::cerr << "** ERROR ** --revoke " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }

        if (args->Present("transfer-owner-session"))
        {
            bool v = args->GetBoolValue("transfer-owner-session", 0);
            conf.SetTransferOwnerSession(v);
            std::cout << "Configuration ownership transfer to session "
                      << (v ? "enabled" : "disabled")
                      << std::endl;
        }

        if (args->Present("lock-down"))
        {
            bool ld = args->GetBoolValue("lock-down", 0);
            conf.SetLockedDown(ld);
            if (ld)
            {
                std::cout << "Configuration has been locked down"
                          << std::endl;
            }
            else
            {
                std::cout << "Configuration has been opened up"
                          << std::endl;

            }
        }


        if (args->Present("public-access"))
        {
            bool ld = args->GetBoolValue("public-access", 0);
            conf.SetPublicAccess(ld);
            if (ld)
            {
                std::cout << "Configuration is now readable to everyone"
                          << std::endl;
            }
            else
            {
                std::cout << "Configuration is now readable to only "
                          << "specific users"
                          << std::endl;

            }
        }

        if (args->Present("seal"))
        {
            std::cout << "This operation CANNOT be undone and makes this "
                      << "configuration profile read-only."
                      << std::endl;
            std::cout << "Are you sure you want to do this? "
                      << "(enter yes in upper case) ";

            std::string response;
            std::cin >> response;
            if ("YES" == response)
            {
                conf.Seal();
                std::cout << "Configuration has been sealed." << std::endl;
            }
            else
            {
                std::cout << "--seal operation has been cancelled"
                          << std::endl;
            }
        }

        if (args->Present("show"))
        {
            std::cout << "    Configuration name: "
                      << conf.GetStringProperty("name")
                      << std::endl;

            std::string owner = lookup_username(conf.GetOwner());
            std::cout << "                 Owner: ("
                      << conf.GetOwner() << ") "
                      << " " << ('(' != owner[0] ? owner : "(unknown)")
                      << std::endl;

            std::cout << "             Read-only: "
                      << (conf.GetBoolProperty("readonly") ? "yes" : "no")
                      << std::endl;

            std::cout << "           Locked down: "
                      << (conf.GetLockedDown() ? "yes" : "no")
                      << std::endl;

            std::cout << "    Ownership transfer: "
                      << (conf.GetTransferOwnerSession() ? "yes" : "no")
                      << std::endl;

            std::cout << "         Public access: "
                      << (conf.GetPublicAccess() ? "yes" : "no")
                      << std::endl;

            if (!conf.GetPublicAccess())
            {
                std::vector<uid_t> acl = conf.GetAccessList();
                std::cout << "  Users granted access: " << std::to_string(acl.size())
                          << (1 != acl.size() ? " users" : " user")
                          << std::endl;
                for (auto const& uid : acl)
                {
                    std::string user = lookup_username(uid);
                    std::cout << "                        - (" << uid << ") "
                              << " " << ('(' != user[0] ? user : "(unknown)")
                    << std::endl;
                }
            }
        }
        return ret;
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  Creates the SingleCommand object for the 'config-acl' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_acl()
{
    //
    //  config-acl command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-acl",
                                "Manage access control lists for configurations",
                                cmd_config_acl));
    auto path_opt = cmd->AddOption("path", 'o', "OBJ-PATH", true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config", 'c', "CONFIG-NAME", true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("show", 's',
                   "Show the current access control lists");
    cmd->AddOption("grant", 'G', "<UID | username>", true,
                   "Grant this user access to this configuration profile");
    cmd->AddOption("revoke", 'R', "<UID | username>", true,
                   "Revoke this user access from this configuration profile");
    cmd->AddOption("public-access", "<true|false>", true,
                   "Set/unset the public access flag",
                   arghelper_boolean);
    cmd->AddOption("transfer-owner-session", 'T', "<true|false>", true,
                   "Transfer the configuration ownership to the VPN session at startup",
                   arghelper_boolean);
    cmd->AddOption("lock-down", "<true|false>", true,
                   "Set/unset the lock-down flag.  Will disable config retrieval for users",
                   arghelper_boolean);
    cmd->AddOption("seal", 'S',
                   "Make the configuration profile permanently read-only");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 config-dump command
 *
 *  This command is used to show the contents of a configuration profile.
 *  It allows both the textual representation, which is compatible with
 *  OpenVPN 2.x based clients as well as JSON by providing the --json option.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_dump(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-dump",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        std::string path = (args->Present("config")
                             ? retrieve_config_path("config-dump",
                                                    args->GetValue("config", 0))
                             : args->GetValue("path", 0));
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);
        if (!conf.CheckObjectExists())
        {
            throw CommandException("config-dump",
                                   "Configuration does not exist");
        }


        if (!args->Present("json"))
        {
            std::cout << conf.GetConfig() << std::endl;
        }
        else
        {
            std::cout << conf.GetJSONConfig() << std::endl;
        }
        return 0;
    }
    catch (DBusException& err)
    {
        throw CommandException("config-dump", err.GetRawError());
    }
}


/**
 *  Creates the SingleCommand object for the 'config-dump' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_dump()
{
    //
    //  config-show command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-dump",
                                "Show/dump a configuration profile",
                                cmd_config_dump));
    cmd->SetAliasCommand("config-show",
                         "**\n** This is command deprecated, use config-dump instead\n**");
    auto path_opt = cmd->AddOption("path", 'o', "OBJ-PATH", true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config", 'c', "CONFIG-NAME", true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("json", 'j', "Dump the configuration in JSON format");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 config-remove command
 *
 *  This command will delete and remove a configuration profile from the
 *  Configuration Manager
 *
 * @param args  ParsedArgs object containing Valid related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_remove(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config"))
    {
        throw CommandException("config-remove",
                               "No configuration profile provided "
                               "(--config, --path)");
    }

    try
    {
        std::string path = (args->Present("config")
                             ? retrieve_config_path("config-remove",
                                                    args->GetValue("config", 0))
                             : args->GetValue("path", 0));

        std::string response;
        if (!args->Present("force"))
        {
            std::cout << "This operation CANNOT be undone and removes this "
                      << "configuration profile completely."
                      << std::endl;
            std::cout << "Are you sure you want to do this? "
                      << "(enter yes in upper case) ";

            std::cin >> response;
        }

        if ("YES" == response || args->Present("force"))
        {
            OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);
            if (!conf.CheckObjectExists())
            {
                throw CommandException("config-remove",
                                       "Configuration does not exist");
            }
            conf.Remove();
            std::cout << "Configuration removed." << std::endl;
        }
        else
        {
            std::cout << "Configuration profile delete operating cancelled"
                      << std::endl;
        }
        return 0;
    }
    catch (DBusException& err)
    {
        throw CommandException("config-remove", err.GetRawError());
    }
    catch (...)
    {
        throw;
    }
}



/**
 *  Creates the SingleCommand object for the 'config-remove' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_config_remove()
{
    //
    //  config-remove command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("config-remove",
                                "Remove an available configuration profile",
                                cmd_config_remove));
    auto path_opt = cmd->AddOption("path", 'o', "OBJ-PATH", true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_config_paths);
    path_opt->SetAlias("config-path");
    cmd->AddOption("config", 'c', "CONFIG-NAME", true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names);
    cmd->AddOption("force",
                   "Force the deletion process without asking for confirmation");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
