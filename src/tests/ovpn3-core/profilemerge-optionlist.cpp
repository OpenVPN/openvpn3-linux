//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2019 - 2022  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   profilemerge-optionlist.cpp
 *
 * @brief  Simple test program importing a configuration file
 *         and querying it for specific options
 */

#include "config.h"

#include <iostream>

#define USE_TUN_BUILDER
#include "openvpn/log/logsimple.hpp"
#include <client/ovpncli.cpp>

#include "common/cmdargparser.hpp"
#include "common/core-extensions.hpp"

using namespace openvpn;


int cmd_pm_optlist_test(ParsedArgs::Ptr args)
{
    try
    {
        args->Present({"config", "option", "dump", "dump-json"});
    }
    catch (const OptionNotFound &e)
    {
        throw CommandException("pm+optlist-test", "Missing options");
    }

    // Parse the OpenVPN configuration
    // The ProfileMerge will ensure that all needed
    // files are embedded into the configuration we
    // send to and store in the Configuration Manager
    ProfileMerge pm(args->GetValue("config", 0),
                    "",
                    "",
                    ProfileMerge::FOLLOW_FULL,
                    ProfileParseLimits::MAX_LINE_SIZE,
                    ProfileParseLimits::MAX_PROFILE_SIZE);

    // Basic profile limits
    OptionList::Limits limits("profile is too large",
                              ProfileParseLimits::MAX_PROFILE_SIZE,
                              ProfileParseLimits::OPT_OVERHEAD,
                              ProfileParseLimits::TERM_OVERHEAD,
                              ProfileParseLimits::MAX_LINE_SIZE,
                              ProfileParseLimits::MAX_DIRECTIVE_SIZE);

    // Try to find the option - if found, print it
    try
    {
        OptionListJSON opts;
        opts.parse_from_config(pm.profile_content(), &limits);
        opts.parse_meta_from_config(pm.profile_content(), "OVPN_ACCESS_SERVER", &limits);
        opts.update_map();

        if (args->Present("option"))
        {
            Option pt = opts.get(args->GetValue("option", 0));
            std::cout << "FOUND: " << pt.printable_directive() << std::endl;
        }
        else if (args->Present("dump"))
        {
            std::cout << opts.string_export() << std::endl;
        }
        else if (args->Present("dump-json"))
        {
            std::cout << opts.json_export() << std::endl;
        }
        return 0;
    }
    catch (std::exception &e)
    {
        std::cout << "Not found: " << e.what() << std::endl;
        return 1;
    }
}



int main(int argc, char **argv)
{
    try
    {
        SingleCommand argparser(argv[0], "ProfileMerge+OptionList tester", cmd_pm_optlist_test);
        argparser.AddOption("config", 'c', "FILE", true, "Configuration file to process");
        argparser.AddOption("option", 'o', "CONFIG-OPTION", true, "Configuration option to extract and print");
        argparser.AddOption("dump", "Dump parsed config to console");
        argparser.AddOption("dump-json", "Dump parsed config to console as JSON");

        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException &e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
        return 2;
    }
}
