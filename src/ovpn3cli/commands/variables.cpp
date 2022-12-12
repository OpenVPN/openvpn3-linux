//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2021 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2021 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   variables.cpp
 *
 * @brief  Retrieve various OpenVPN 3 Linux runtime variables
 */

#include <iostream>
#include "common/cmdargparser.hpp"

#ifdef OVPN3CLI_OPENVPN3ADMIN
#include "common/machineid.hpp"


/**
 *  Dump various runtime variables used by OpenVPN 3 Linux
 *
 * @param args  ParsedArgs object containing all related options and arguments
 *
 * @return Returns the exit code which will be returned to the calling shell
 */
int cmd_variables(ParsedArgs::Ptr args)
{
    if (args->Present("machine-id"))
    {
        MachineID machine_id;
        try
        {
            std::cout << "Machine-ID (IV_HWADDR): " << machine_id << std::endl
                      << "Source: ";
            switch (machine_id.GetSource())
            {
            case MachineID::SourceType::SYSTEM:
                std::cout << "System";
                break;
            case MachineID::SourceType::SYSTEMD_API:
                std::cout << "systemd API";
                break;
            case MachineID::SourceType::LOCAL:
                std::cout << "Installation local";
                break;
            case MachineID::SourceType::RANDOM:
                std::cout << "Random - unreliable";
                break;
            case MachineID::SourceType::NONE:
                std::cout << "Unknown/None - unreliable";
                break;
            }
            std::cout << std::endl;

            machine_id.success();
        }
        catch (const MachineIDException &excp)
        {
            std::cerr << "** ERROR ** " << excp.GetError() << std::endl;
            return 1;
        }
    }

    return 0;
}
#endif // OVPN3CLI_OPENVPN3ADMIN

/**
 *  Creates the SingleCommand object for the 'variables' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_variables()
{
    SingleCommand::Ptr variables;
#ifdef OVPN3CLI_OPENVPN3ADMIN
    variables.reset(new SingleCommand("variables",
                                      "Show various OpenVPN 3 variables",
                                      cmd_variables));
    variables->AddOption("machine-id",
                         "Show the Machine-ID variable sent to the OpenVPN server");

#endif // OVPN3CLI_OPENVPN3ADMIN
    return variables;
}
