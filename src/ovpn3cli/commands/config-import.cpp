//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   config-import.hpp
 *
 * @brief  Implementation of the openvpn3 config-import command
 */

#include "config.h"

#define USE_TUN_BUILDER
#include <client/ovpncli.cpp>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "configmgr/proxy-configmgr.hpp"
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
    ProfileMerge pm(filename,
                    "",
                    "",
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
    std::string cfgpath = conf.Import(cfgname,
                                      pm.profile_content(),
                                      single_use,
                                      persistent);

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
        const ValidOverride &vo = cfgprx.LookupOverride("persist-tun");
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
    catch (std::exception &e)
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
    cmd->AddOption("config",
                   'c',
                   "CFG-FILE",
                   true,
                   "Configuration file to import");
    cmd->AddOption("name",
                   'n',
                   "NAME",
                   true,
                   "Provide a different name for the configuration (default: CFG-FILE)");
    cmd->AddOption("persistent",
                   'p',
                   "Make the configuration profile persistent through service restarts");

    return cmd;
}
