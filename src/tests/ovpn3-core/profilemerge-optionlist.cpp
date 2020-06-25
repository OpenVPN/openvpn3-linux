//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   profilemerge-optionlist.cpp
 *
 * @brief  Simple test program importing a configuration file
 *         and querying it for specific options
 */

#include "config.h"

#include <iostream>

#define USE_TUN_BUILDER
#include <client/ovpncli.cpp>

#include "common/cmdargparser.hpp"

using namespace openvpn;

int cmd_pm_optlist_test(ParsedArgs::Ptr args)
{
    if (!args->Present("config") || !args->Present("option"))
    {
        throw CommandException("pm+optlist-test", "Missing --config and/or --option");
    }

    // Parse the OpenVPN configuration
    // The ProfileMerge will ensure that all needed
    // files are embedded into the configuration we
    // send to and store in the Configuration Manager
    ProfileMerge pm(args->GetValue("config", 0), "", "",
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
        OptionList opts;
        opts.parse_from_config(pm.profile_content(), &limits);
        opts.update_map();
        Option pt = opts.get(args->GetValue("option", 0));
        std::cout << "FOUND: " << pt.printable_directive() << std::endl;
        return 0;
    }
    catch (std::exception& e)
    {
        std::cout << "Not found: " << e.what() << std::endl;
        return 1;
    }
}


int main(int argc, char **argv)
{
    try
    {
        SingleCommand argparser(argv[0], "ProfileMerge+OptionList tester",
                                cmd_pm_optlist_test);
        argparser.AddOption("config", 'c', "FILE", true,
                            "Configuration file to process");
        argparser.AddOption("option", 'o', "CONFIG-OPTION", true,
                            "Configuration option to extract and print");

        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException& e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
        return 2;
    }
}



