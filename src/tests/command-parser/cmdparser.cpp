//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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
 * @file   cmdparser.cpp
 *
 * @brief  Simple test for the command line parser used by openvpn3.cpp
 */


#include <iostream>
#include <sstream>
#include <random>
#include <cstring>

#include "common/cmdargparser.hpp"

int cmd_dump_arg_test(ParsedArgs::Ptr args)
{
    std::cout << "===> cmd_dump_arg_test()" << std::endl << std::endl;;

    for (auto const& key : args->GetOptionNames())
    {
        std::cout << "[cmd_test1] Argument: " << key << " = [";
        int i = 0;
        for (auto const& val : args->GetAllValues(key))
        {
            if (i > 0)
            {
                std::cout << ", ";
            }
            std::cout << "\"" << val << "\"";
            i++;
        }
        std::cout << "]" << std::endl;
    }
    auto extra = args->GetAllExtraArgs();
    std::cout << "[cmd_test1] Extra argument counts: "
              << extra.size() << std::endl;
    for (auto const& a : args->GetAllExtraArgs())
    {
        std::cout << "[cmd_test1] Extra: " << a << std::endl;
    }
    return 0;
}


int cmd_multiply(ParsedArgs::Ptr args)
{
    std::cout << "===> cmd_multiply() test" << std::endl << std::endl;

    if (args->Present("bool-test"))
    {
        std::cout << "Boolean test: "
                  << (args->GetBoolValue("bool-test", 0) ? "true" : "false")
                  << std::endl;
    };

    unsigned long long res = 0;
    std::cout << "Multiplying ... ";
    for (auto const& v : args->GetAllValues("multiply"))
    {
        if (0 == res)
        {
            res = (atol(v.c_str()));
        }
        else
        {
            std::cout << ", ";
            res *= (atol(v.c_str()));
        }
        std::cout << v;
    }
    std::cout << " ... Done" << std::endl;
    std::cout << "Final result: " << std::to_string(res) << std::endl;
    return 0;
}


std::string arghelper_mandatory_arg()
{
    return "'mandatory_string'";
}


std::string arghelper_random_numbers()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());

    std::uniform_int_distribution<unsigned short> distrib;

    std::stringstream res;
    for (int i = 0; i < 5; i++)
    {
        res << std::to_string(distrib(gen)) << " ";
    }
    return res.str();
}


std::string arghelp_boolean()
{
    return "false true";
}


int main(int argc, char **argv)
{
    Commands cmds("Command line parser test",
                  "Simple example and test tool for the command line parser");

    SingleCommand::Ptr test1_cmd;
    test1_cmd.reset(new SingleCommand("test1", "Test command 1",
                                      cmd_dump_arg_test));
    test1_cmd->AddOption("set-value", 's',
                         "key", true, "Set a variable");
    test1_cmd->AddOption("test-func1", "Just testing more options");
    test1_cmd->AddOption("test-func2", 'f', "string", false, "Just another test");
    test1_cmd->AddOption("mandatory-arg", "string", true, "Test mandatory option argument",
                         arghelper_mandatory_arg);
    test1_cmd->AddVersionOption();
    cmds.RegisterCommand(test1_cmd);

    SingleCommand::Ptr test2_cmd;
    test2_cmd.reset(new SingleCommand("test2", "Test command two",
                                      cmd_multiply));
    test2_cmd->AddOption("multiply", 'm', "values" , true, "Multiply two numbers",
                         arghelper_random_numbers);
    test2_cmd->AddOption("bool-test", 'b',
                         "<true|false>", true, "Test of a boolean option",
                         arghelp_boolean);
    cmds.RegisterCommand(test2_cmd);

    SingleCommand::Ptr test3_cmd;
    test3_cmd.reset(new SingleCommand("test3", "Test command 3",
                                           cmd_dump_arg_test));
    test3_cmd->AddOption("opt-string", 'o',
                         "string-1", false, "Optional strings");
    cmds.RegisterCommand(test3_cmd);

    try
    {
        return cmds.ProcessCommandLine(argc, argv);
    }
    catch (CommandException& e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
    }

}
