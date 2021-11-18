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
 * @file   cmdargparser.cpp
 *
 * @brief  Command line argument parser for C++.  Built around getopt_long()
 */

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <glib-unix.h>
#include <json/json.h>

#include <openvpn/common/rc.hpp>
using namespace openvpn;

#include "common/utils.hpp"
#include "common/cmdargparser.hpp"
#include "common/configfileparser.hpp"

//
//  ParsedArgs implementation
//

void ParsedArgs::ImportConfigFile(Configuration::File::Ptr config)
{
    for (const auto& opt : config->GetOptions())
    {
        // Check if this is an exclusive option
        std::vector<std::string> related = config->GetRelatedExclusiveOptions(opt);
        // Remove all the related exclusive options in the parsed args.
        // The imported configuration file overrides the command line
        // arguments
        for (const auto& rel : related)
        {
            remove_arg(rel);
        }

        // Add/overwrite the argument/option from the configuration file
        remove_arg(opt);
        key_value[opt].push_back(config->GetValue(opt));
        present.push_back(opt);
    }
}


//
//  Commands::ShellCompletion implementation
//

Commands::ShellCompletion::ShellCompletion()
    : SingleCommand("shell-completion",
                    "Helper function to provide shell completion data",
                    nullptr)
{
    AddOption("list-commands",
              "List all available commands");
    AddOption("list-options", "COMMAND", true,
              "List all available options for a specific command");
    AddOption("arg-helper", "OPTION", true,
              "Used together with --list-options, lists value hint to an option");
}

/**
 *  Provide a "back-pointer" to the parent Commands objects.  This
 *  is needed to be able to extract all the various commands,
 *  options and the argument helper function in each of the registered
 *  commands.
 *
 * @param cmds  Commands * to the parent Commands object.
 */
void Commands::ShellCompletion::SetMainCommands(Commands * cmds)
{
    commands = cmds;
}


int Commands::ShellCompletion::RunCommand(const std::string arg0, unsigned int ignored_skip,
               int argc, char **argv)
{
    try {
        ParsedArgs::Ptr args = parse_commandline(arg0, 1, argc, argv);

        if (!args->GetCompleted())
        {
            return 0;
        }

        if (args->Present("list-commands")
            && args->Present("list-options"))
        {
            throw CommandException("shell-completion",
                                   "Cannot combine --list-commands and --list-options");
        }

        if (args->Present("list-options") && args->GetValueLen("list-options") > 1)
        {
            throw CommandException("shell-completion",
                                   "Can only use --list-options once");
        }


        if (args->Present("arg-helper")
            && !args->Present("list-options"))
        {
            throw CommandException("shell-completion",
                                   "--arg-helper requires --list-options");
        }

        if (args->Present("list-commands"))
        {
            list_commands();
            return 0;
        }

        if (args->Present("list-options"))
        {
            if (!args->Present("arg-helper"))
            {
                list_options(args->GetValue("list-options", 0));
            }
            else
            {
                call_arg_helper(args->GetValue("list-options", 0),
                                args->GetValue("arg-helper", 0));
            }
        }
    }
    catch (...)
    {
        throw;
    }
    return 3;
}


void Commands::ShellCompletion::list_commands()
{
    bool first = true;
    for (auto &cmd : commands->GetAllCommandObjects())
    {
        if (cmd->GetCommand() == GetCommand())
        {
            // Skip myself
            continue;
        }
        if (!first)
        {
            std::cout << " ";
        }
        first = false;
        std::cout << cmd->GetCommand();
    }
    std::cout << std::endl;
}


/**
 *  Generate the list of options for a specific command.  This
 *  job is done by the SingleCommand object itself, through the
 *  SingleCommand::GetOpttionList() method.  The result is written
 *  straight to stdout.
 *
 * @param cmd  std::string containing the command to query for
 *             available options.
 */
void Commands::ShellCompletion::list_options(const std::string cmd)
{
    for (auto const& c : commands->GetAllCommandObjects())
    {
        if ((c->GetCommand() == cmd) || (c->GetAliasCommand() == cmd))
        {
            std::cout << c->GetOptionsList() << std::endl;
            return;
        }
    }
}


void Commands::ShellCompletion::call_arg_helper(const std::string cmd, const std::string option)
{
    for (auto const& c : commands->GetAllCommandObjects())
    {
        if ((c->GetCommand() == cmd) || (c->GetAliasCommand() == cmd))
        {
            unsigned int skip = 0;
            if ('-' == option[0])
            {
                skip++;
            }
            if ('-' == option[1])
            {
                skip++;
            }
            std::cout << c->CallArgumentHelper(option.substr(skip))
                      << std::endl;
            return;
        }
    }
}



//
//  ParsedArgs implementation
//

void ParsedArgs::CheckExclusiveOptions(const ExclusiveGroups& args_exclusive) const
{
    // Check if processed options is listed as an exclusive argument
    for (const auto& arg : present)
    {
        for (const auto& group : args_exclusive)
        {
            // Is this argument listed in this group of exclusive args?
            if (std::find(group.begin(), group.end(), arg) == group.end())
            {
                // Not found in this group, continue to next group
                continue;
            }

            // This argument was found to be in an exclusiveness group.
            // Now we need to see if any of the other arguments in this
            // group has been used already.
            for (const auto& x : group)
            {
                if (arg == x)
                {
                    // Don't check against itself
                    continue;
                }

                // Check if any of the other options in this group in
                // the exclusiveness list has been used already
                if (std::find(present.begin(),
                              present.end(),
                              x) != present.end())
                {
                    throw ExclusiveOptionError(arg, group);
                }
            }
        }
    }  // End of exclusiveness check
}


bool ParsedArgs::parse_bool_value(const std::string& k, const std::string& value) const
{
    if (("false" != value) && ("true" != value )
        && ("no" != value) && ("yes" != value))
    {
        throw OptionException(k, "Boolean options must be either 'false' or 'true'");
    }
    return "true" == value || "yes" == value;
}


void ParsedArgs::remove_arg(const std::string& opt)
{
    // Remove the parsed argument values
    auto e = key_value.find(opt);
    if (e != key_value.end())
    {
        e->second = std::vector<std::string>{};
    }

    // Remove the item from the list of present arguments/options
    auto p = std::find(present.begin(), present.end(), opt);
    if (p != present.end())
    {
        present.erase(p);
    }
}


//
//  RegisterParsedArgs implementation
//

void RegisterParsedArgs::register_option(const std::string k, const char *v)
{
    if (NULL != v)
    {
        key_value[k].push_back(std::string(v));
    }
    for (auto const& e : present)
    {
        // Don't register duplicates
        if (e == k) {
            return;
        }
    }
    present.push_back(k);
}


void RegisterParsedArgs::register_extra_args(const char * e)
{
    extra_args.push_back(std::string(e));
}


void RegisterParsedArgs::set_completed()
{
    completed = true;
}



//
//  SingleCommandOption implementation
//

SingleCommandOption::SingleCommandOption(const std::string longopt,
                                         const char shrtopt,
                                         const std::string help_text)
    : longopt(longopt), shortopt(shrtopt),
      metavar(""), arg_helper_func(nullptr),
      help_text(help_text)
{
    update_getopt(longopt, shortopt, no_argument);
}


SingleCommandOption::SingleCommandOption(const std::string longopt,
                                         const char shrtopt,
                                         const std::string metavar,
                                         const bool required,
                                         const std::string help_text,
                                         const argHelperFunc arg_helper_func)
    : longopt(longopt), shortopt(shrtopt),
      metavar(metavar),  arg_helper_func(arg_helper_func),
      help_text(help_text),
      alias("")
{
    update_getopt(longopt, shortopt,
                  (required ? required_argument : optional_argument));
}


SingleCommandOption::~SingleCommandOption()
{
    // We need to override this, as we're allocating this
    // const char * struct member dynamically while
    // getopt typically expects this to be static.  This
    // is allocated in init_getopt()
    free((char *)getopt_option.name);

    if (getopt_alias.name)
    {
        free((char *)getopt_alias.name);
    }
}


void SingleCommandOption::SetAlias(const std::string& optalias)
{
    if (!alias.empty())
    {
        throw CommandException(alias, "Alias already registered");
    }

    alias = optalias;

    //  Initialise a separate struct option for the alias
    getopt_alias.name = strdup(alias.c_str());
    getopt_alias.flag = NULL;
    getopt_alias.has_arg =  getopt_option.has_arg;
    getopt_alias.val = 0;
}


std::string SingleCommandOption::get_option_list_prefixed()
{
    std::stringstream r;

    if (0 != shortopt)
    {
        r << "-" << shortopt;

    }
    if (!longopt.empty())
    {
        if (r.tellp() != std::streampos(0))
        {
            r << " ";
        }
        r << "--" << longopt;

        if (optional_argument == getopt_option.has_arg)
        {
            r << " --" << longopt << "=";
        }
    }
    if (!alias.empty())
    {
        if (r.tellp() != std::streampos(0))
        {
            r << " ";
        }
        r << "--" << alias;
        if (optional_argument == getopt_alias.has_arg)
        {
            r << " --" << alias << "=";
        }
    }
    return r.str();
}


std::string SingleCommandOption::call_argument_helper_callback(const std::string& opt_name)
{
    if (nullptr == arg_helper_func)
    {
        return "";
    }

    // If this option has optional arguments, only suggest values if the option
    // has a trailing '='
    if ((optional_argument == getopt_option.has_arg
         || optional_argument == getopt_alias.has_arg)
        && (opt_name.rfind("=") == std::string::npos))
    {
        return "";
    }

    return arg_helper_func();
}


std::string SingleCommandOption::getopt_optstring()
{
    if (0 == shortopt)
    {
        return "";
    }

    std::stringstream ret;
    ret << shortopt;

    switch (getopt_option.has_arg)
    {
    case optional_argument:
        ret << ":";
        // The fall-through is by design.
        // optional arguments should have two colons
    case required_argument:
        ret << ":";
        break;
    default:
        break;
    }

    return ret.str();
}


bool SingleCommandOption::check_short_option(const char o)
{
    return o == shortopt;
}


bool SingleCommandOption::check_long_option(const char *o)
{
    if (NULL != o)
    {
        return (std::string(o) == longopt || std::string(o) == alias);
    }
    return false;
}


std::string SingleCommandOption::get_option_name()
{
    return longopt;
}


std::vector<struct option *> SingleCommandOption::get_struct_option()
{
    std::vector<struct option *> ret;
    ret.push_back(&getopt_option);

    // Add the alias, if configured
    if (!alias.empty())
    {
        ret.push_back(&getopt_alias);
    }
    return ret;
}


std::vector<std::string> SingleCommandOption::gen_help_line(const unsigned int width)
{
    std::vector<std::string> ret;
    ret.push_back(gen_help_line_generator(shortopt, longopt,
                                          help_text, width));
    if (!alias.empty())
    {
        std::stringstream alias_help;
        alias_help << "Alias for --" << longopt;
        ret.push_back(gen_help_line_generator(0, alias,
                                              alias_help.str(), width));
    }
    return ret;
}


std::string SingleCommandOption::gen_help_line_generator(const char opt_short,
                                    const std::string& opt_long,
                                    const std::string& opt_help,
                                    const unsigned int width)
{
    std::stringstream r;

    // If we don't have a short option, fill out with blanks to
    // have the long options aligned under each other
    if (0 == opt_short)
    {
        r << "     ";
    }
    else  // ... we have a short option, format the output of it
    {
        r << "-" << opt_short<< " | ";
    }
    r << "--" << opt_long;

    // If this option can process a provided value ...
    if (!metavar.empty())
    {
        // If this value is mandatory, don't include [] around the
        // the argument value.
        if (required_argument == getopt_option.has_arg)
        {
            r << " " << metavar;
        }
        else
        {
            // This option is optional, indicate it by embracing the
            // value description into [].
            r << "[=" << metavar << "]";
        }
    }

    // Ensure the description is aligned with the rest of the lines
    size_t l = r.str().size();

    if ((width - 3) < l)
    {
        // If this first line with the long/short option listing gets too
        // long, then split it up into several lines.
        r << std::endl;

        // Set the needed spacing to the description column
        r << std::setw(width) << " ";
    }
    else
    {
        // Set the needed spacing to the description column
        r << std::setw(width - l);
    }
    r << " - " << opt_help;

    return r.str();
}


void SingleCommandOption::update_getopt(const std::string longopt, const char  shortopt,
                                        const int has_args)
{
    // We need a strdup() as the pointer longopt.c_str() returns on
    // some systems gets destroyed - rendering garbage in this
    // struct.  In the destructor, this is free()d again.
    getopt_option.name = strdup(longopt.c_str());
    getopt_option.flag = NULL;
    getopt_option.has_arg =  has_args;
    getopt_option.val = shortopt;

    // Ensure the alias is properly initialized
    getopt_alias.name = nullptr;
}


SingleCommandOption::Ptr SingleCommand::AddOption(const std::string longopt,
                                                  const char shortopt,
                                                  const std::string help_text)
{
    SingleCommandOption::Ptr opt = new SingleCommandOption(longopt,
                                                           shortopt,
                                                           help_text);
    options.push_back(opt);
    return opt;
}

SingleCommandOption::Ptr SingleCommand::AddOption(const std::string longopt,
                                                  const char shortopt,
                                                  const std::string metavar,
                                                  const bool required,
                                                  const std::string help_text,
                                                  const argHelperFunc arg_helper)
{
    SingleCommandOption::Ptr opt = new SingleCommandOption(longopt,
                                                           shortopt,
                                                           metavar,
                                                           required,
                                                           help_text,
                                                           arg_helper);
    options.push_back(opt);
    return opt;
}


void SingleCommand::SetAliasCommand(const std::string& alias,
                                    const std::string& remark)
{
    alias_cmd = alias;
    alias_remark = remark;
}


const std::string SingleCommand::GetAliasCommand() const
{
    return alias_cmd;
}


void SingleCommand::AddVersionOption(const char shortopt)
{
    (void) AddOption("version", shortopt, "Show version information");
    opt_version_added = true;
}


std::string SingleCommand::GetCommandHelp(unsigned int width)
{
    std::stringstream ret;
    ret << command << std::setw(width - command.size())
        << " - " << description << std::endl;
    return ret.str();
}


std::string SingleCommand::GetOptionsList()
{
    std::stringstream r;
    for (auto const& opt : options)
    {
        r << opt->get_option_list_prefixed() << " ";
    }
    return r.str();
}


std::string SingleCommand::CallArgumentHelper(const std::string option_name)
{
    for (auto const& opt : options)
    {
        // Strip any trailing '=' for the long option check.
        // Options with optional arguments always has a '=' to provide a value
        auto pos = option_name.rfind("=");
        std::string o = (pos != std::string::npos ? option_name.substr(0, pos) : option_name);

        if (1 == option_name.size()
            && opt->check_short_option(option_name[0]))
        {
            return opt->call_argument_helper_callback(option_name);
        }
        else if (opt->check_long_option(o.c_str()))
        {
            return opt->call_argument_helper_callback(option_name);
        }
    }
    return "";
}


int SingleCommand::RunCommand(const std::string arg0, unsigned int skip,
                              int argc, char **argv)
{
    try {
        ParsedArgs::Ptr cmd_args = parse_commandline(arg0, skip, argc, argv);

        // Run the callback function.
        return cmd_args->GetCompleted() ? command_func(cmd_args) : 0;
    }
    catch (...)
    {
        throw;
    }
}


RegisterParsedArgs::Ptr SingleCommand::parse_commandline(const std::string & arg0,
                                                         unsigned int skip,
                                                         int argc, char **argv)
{
    struct option *long_opts = init_getopt();

    if (argv[1] && (argv[1] == alias_cmd) && !alias_remark.empty())
    {
        std::cout << alias_remark << std::endl;
    }

    RegisterParsedArgs::Ptr cmd_args;
    cmd_args.reset(new RegisterParsedArgs(arg0));
    int c;
    optind = 1 + skip; // Skip argv[0] which contains this command name
    try
    {
        while (1)
        {
            int optidx = 0;
            c = getopt_long(argc, argv, shortopts.c_str(), long_opts, &optidx);
            if (-1 == c)  // Are we done?
            {
                break;
            }

            // If -h or --help is used, print the help screen and exit
            if ('h' == c)
            {
                std::cout << gen_help(arg0) << std::endl;
                goto exit;
            }

            // If an unknown option is detected
            if ('?' == c)
            {
                throw CommandException(command);
            }

            // Check if this matches an option which has been defined
            if (0 == c)
            {
                // No match on short option ... search on long option,
                // based on optidx

                if (opt_version_added
                    && (0 == strncmp("version", long_opts[optidx].name, 7)))
                {
                    std::cout << get_version(arg0) << std::endl;
                    goto exit;
                }

                for (auto& o : options)
                {
                    if (o->check_long_option(long_opts[optidx].name))
                    {
                            cmd_args->register_option(o->get_option_name(), optarg);
                    }
                }
            }
            else
            {
                // Match on short option ... search on short option
                for (auto& o : options)
                {
                    if (o->check_short_option(c))
                    {
                        cmd_args->register_option(o->get_option_name(), optarg);
                    }
                }
            }
        }

        // If there are still arguments not parsed, gather them all.
        if (optind < argc)
        {
            // All additional arguments gets saved for further
            // processing inside the function to be called
            while (optind < argc)
            {
                cmd_args->register_extra_args(argv[optind++]);
            }
        }
        cmd_args->set_completed();
    }
    catch (...)
    {
        throw;
    }
exit:
    free(long_opts); long_opts = nullptr;
    return cmd_args;
}


struct option *SingleCommand::init_getopt()
{
    unsigned int idx = 0;
    shortopts = "";
    struct option *result = (struct option *) calloc(options.size() + 2,
                                                     sizeof(struct option));
    if (result == NULL)
    {
        throw CommandException(command, "Failed to allocate memory for option parsing");
    }

    // Parse through all registered options
    for (auto const& opt : options)
    {
        // Apply the option definition into the pre-defined designated
        // slot
        for (const auto& o : opt->get_struct_option())
        {
            memcpy(&result[idx], o, sizeof(struct option));
            idx++;
        }

        // Collect all we need for the short flags too
        shortopts += opt->getopt_optstring();
    }

    // getopt_long() expects the last record in the struct option array
    // to be an empty/zeroed struct option element
    result[idx] = {0};
    return result;
}


std::string SingleCommand::gen_help(const std::string arg0)
{
    std::stringstream r;
    r << arg0 << ": " << command << " - " << description << std::endl;
    r << std::endl;

    for (const auto& opt : options)
    {
        for (const auto& l : opt->gen_help_line())
        {
            r << "   " << l << std::endl;
        }
    }
    return r.str();
}



//
//   Commands implementation
//

void Commands::RegisterCommand(const SingleCommand::Ptr cmd)
{
    commands.push_back(cmd);
}


int Commands::ProcessCommandLine(int argc, char **argv)
{
    std::string baseprog = simple_basename(argv[0]);

    if (2 > argc)
    {
        std::cout << "Missing command.  See '" << baseprog
                  << " help' for details"
                  << std::endl;
        return 1;
    }

    // Implicit declaration of the generic help screen.
    std::string cmd(argv[1]);
    if ("help" == cmd || "--help" == cmd || "-h" == cmd)
    {
        print_generic_help(baseprog);
        return 0;
    }

    // Link the ShellCompletion helper object with this Commands object
    // The ShellCompletion sub-class needs access to the commands
    // std::vector to be able to generate the needed strings to be used
    // by the shell completion functions outside of the program itself
    shellcompl->SetMainCommands(this);

    // Find the proper registered command and let that object
    // continue the command line parsing and run the callback function
    for (auto& c : commands)
    {
        // If we found our command ...
        if (c->CheckCommandName(cmd))
        {
            // Copy over the arguments, skip argv[0] and build another one
            // instead.  Ideally, this should not be needed - but
            // getopt_long() needs it for its error reporting.  And
            // removing the error reporting, things gets much more
            // complicated and limited.
            //
            // Also ensure we have one extra element as a NULL terminator
            //
            char **cmdargv = (char **) calloc(sizeof(char *), argc + 1);
            if (NULL == cmdargv)
            {
                throw CommandException(baseprog,
                                       "Could not allocate memory for argument parsing");
            }

            // Build up the new argv[0] to be used instead
            const std::string cmdarg0_str = baseprog + "/" + cmd;
            cmdargv[0] = strdup(cmdarg0_str.c_str());
            int cmdargc = 1;

            // copy over the arguments, ignoring the initial argv[0]
            for (int i = 1; i < argc; i++)
            {
                cmdargv[cmdargc++] = argv[i];
            }

            // Run the command's callback function
            int ec = 1;
            try
            {
                ec = c->RunCommand(argv[0], 1, cmdargc, cmdargv);
            }
            catch (...)
            {
                // If something happened when running the
                // callback function, clean-up and pass the same
                // exception further.
                free(cmdargv[0]);
                free(cmdargv);
                throw;
            }
            free(cmdargv[0]);
            free(cmdargv);
            return ec;
        }
    }

    std::cout << argv[0] << ": Unknown command '" << cmd << "'" << std::endl;
    return 1;
}


std::vector<SingleCommand::Ptr> Commands::GetAllCommandObjects()
{
    return commands;
}


void Commands::print_generic_help(std::string arg0)
{
    std::cout << arg0 << ": " << progname << std::endl;
    std::cout << std::setw(arg0.size()+2) << " " << description;
    std::cout << std::endl << std::endl;

    std::cout << "  Available commands: " << std::endl;

    unsigned int width = 20;
    std::cout << "    help" << std::setw(width - 7) << " "
              << " - This help screen"
              << std::endl;

    for (auto &cmd :commands)
    {
        std::cout << "    " << cmd->GetCommandHelp(width);
    }
    std::cout << std::endl;

    std::cout << "For more information, run: "
              << arg0 << " <command> --help"
              << std::endl;
    std::cout << std::endl;
}
