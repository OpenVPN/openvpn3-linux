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
 * @file   cmdparser.cpp
 *
 * @brief  Simple test for the command line parser used by openvpn3.cpp
 */


#include <iostream>
#include <sstream>

#include "../../ovpn3cli/cmdargparser.hpp"

int cmd_dump_arg_test(ParsedArgs args)
{
    std::cout << "===> cmd_dump_arg_test()" << std::endl << std::endl;;

    for (auto const& key : args.GetOptionNames())
    {
        std::cout << "[cmd_test1] Argument: " << key << " = [";
        int i = 0;
        for (auto const& val : args.GetAllValues(key))
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
    for (auto const& a : args.GetAllExtraArgs())
    {
        std::cout << "[cmd_test1] Extra: " << a << std::endl;
    }
    return 0;
}


int cmd_multiply(ParsedArgs args)
{
    std::cout << "===> cmd_multiply() test" << std::endl << std::endl;

    unsigned long long res = 0;
    std::cout << "Multiplying ... ";
    for (auto const& v : args.GetAllValues("multiply"))
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


int main(int argc, char **argv)
{
    Commands cmds("Command line parser test",
                  "Simple example and test tool for the command line parser");

    auto test1_cmd = cmds.AddCommand("test1", "Test command 1", cmd_dump_arg_test);
    test1_cmd->AddOption("set-value", 's',
                         "key", true, "Set a variable");
    test1_cmd->AddOption("test-func1", "Just testing more options");
    test1_cmd->AddOption("test-func2", 'f', "string", false, "Just another test");
    test1_cmd->AddOption("mandatory-arg", "string", true, "Test mandatory option argument");

    auto test2_cmd = cmds.AddCommand("test2", "Test command two", cmd_multiply);
    test2_cmd->AddOption("multiply", 'm', "values" , true, "Multiply two numbers");

    auto test3_cmd = cmds.AddCommand("test3", "Test command 3", cmd_dump_arg_test);
    test3_cmd->AddOption("opt-string", 'o',
                         "string-1", false, "Optional strings");

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

