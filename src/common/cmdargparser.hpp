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
 * @file   cmdargparser.hpp
 *
 * @brief  Command line argument parser for C++.  Built around getopt_long()
 */

#pragma once

#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <glib-unix.h>

#include <openvpn/common/rc.hpp>
using namespace openvpn;

#include "common/cmdargparser-exceptions.hpp"
#include "common/configfileparser.hpp"
#include "common/utils.hpp"


/**
 *  This class is sent to the callback functions which is called
 *  when parsing the arguments and options.  A ParsedArgs object contains
 *  all the parsed options and their arguments through a simple API.
 *
 *  This class is not directly populated, but this happens via internal
 *  RegisterParsedArgs class which inherits this class (hence the protected
 *  variables).
 */
class ParsedArgs
{
public:
    typedef std::shared_ptr<ParsedArgs> Ptr;
    typedef std::vector<std::vector<std::string>> ExclusiveGroups;

    ParsedArgs() : argv0("") {}

    ParsedArgs(const std::string argv0)
        : argv0(argv0)
    {
    }

    virtual ~ParsedArgs() = default;

    /**
     *  Check if the command line parser completed parsing all options and
     *  arguments.
     *
     * @return Boolean value.  If true, the parser completed and the callback
     *         function may be called.
     */
    bool GetCompleted() const
    {
        return completed;
    }

    /**
     *   Import option settings from a configuration file
     *
     * @param config     Configuration::File::Ptr to the configuration
     *                   file parser
     *
     * @throws ExclusiveOptionError if overrwite is false and an exclusive
     *         option from the command line conflicts with the configuration
     *         file.
     */
    void ImportConfigFile(Configuration::File::Ptr config);

    /**
     *  Get the program name (argv[0])
     * @return
     */
    std::string GetArgv0() const
    {
        return argv0;
    }


    /**
     *  Check if parsed options are exclusive according to the ExclusiveGroups
     *
     *  The ExclusiveGroups is essentially an array of grouped options, where
     *  each group is processed independently but the option inside a single
     *  group will trigger an exception.
     *
     *  Example:
     *
     *      CheckExclusiveOptions({{"arg-1", "arg-2", "arg-3"},
     *                             {"opt-A", "opt-B", "opt-C"}});
     *
     *  If a program is started with the following list, it is fine:
     *        --arg-2 --opt-C --verbose --bla-bla
     *
     *  Any of the examples below will throw an exception (one line is
     *  one example)
     *
     *        --arg-1 --arg-3
     *        --arg-2 --arg-4 --opt-B --opt-A
     *        --arg-1 --arg-2 --arg-3
     *
     * @param args_exclusive  ExclusiveGroups containing groups of options
     *                        which cannot be combined.
     *
     * @throws ExclusiveOptionError if an option is not exclusive
     */
    void CheckExclusiveOptions(const ExclusiveGroups& args_exclusive) const;

    /**
     *  Checks if a specific option name has been parsed.  This is
     *  useful for options which does not take any additional argument.  This
     *  is typically used when setting various flags from the command line
     *
     * @param k  std::string containing the option to look-up.
     * @return Returns true the option has been processed and found at the
     *         command line.  Otherwise false.
     */
    bool Present(const std::string k) const
    {
        for (auto const& e : present)
        {
            if (k == e)
            {
                return true;
            }
        }
        return false;
    }


    /**
     *  Extension of @Present(const std::string&) which will parse a list
     *  of options to see if one of them are present.  It will return on the
     *  first match.
     *
     * @param  optlist  std::vector<std::string> of options to check for
     * @return Returns std::string of the first matching option.
     * @throws OptionsNotFound if no option was found
     */
    const std::string Present(const std::vector<std::string> optlist) const
    {
        for (const auto& k : optlist)
        {
            if (Present(k))
            {
                return k;
            }
        }
        throw OptionNotFound();
    }


    /**
     *  Retrieve a vector with all the various option names found during
     *  the command line argument parsing.
     *
     * @return Returns a std::vector<std::string> of all unique options parsed.
     */
    std::vector<std::string> GetOptionNames() const
    {
        std::vector<std::string> keys;
        for (auto const& k: present)
        {
            keys.push_back(k);
        }
        return keys;
    }


    /**
     *  Retrieve the number of value elements found for a specific key.
     *  If the same option is provided on the command line more than once,
     *  they are gathered into the same option container.
     *
     * @param k  std::string containing the option name to look-up
     * @return Returns the number of elements found for that option name.
     */
    unsigned int GetValueLen(const std::string k) const noexcept
    {
        try
        {
            return key_value.at(k).size();
        }
        catch (const std::out_of_range&)
        {
            // Ignore out-of-bound errors
            return 0;
        }
    }


    /**
     *  Retrieve a specific option, based on option name and value index
     *
     * @param k    std::string containing the option name to look-up
     * @param idx  unsigned int of the value element to retrieve
     * @return  Returns a std::string with the collected value
     */
    std::string GetValue(const std::string k, const unsigned int idx) const
    {
        return key_value.at(k).at(idx);
    }


    /**
     *  Retrieve the last value of a specific option name
     *
     *   This is useful when you only care about the last time an
     *   option argument was given.  This ensures the last option usage
     *   overrides the prior settings.
     *
     * @param k   std::string containing the option name to look-up
     * @return    Returns a std::string with the collected value
     */
    std::string GetLastValue(const std::string& k) const
    {
        return key_value.at(k).back();
    }


    /**
     *  Retrieve a specific boolean option, based on option name and value
     *  index
     *
     * @param idx  unsigned int of the value element to retrieve
     * @param k    std::string containing the option name to look-up
     * @return  Returns a bool with the collected value
     */
    bool GetBoolValue(const std::string k, const unsigned int idx) const
    {
        return parse_bool_value(k, key_value.at(k).at(idx));
    }


    /**
     *  Retrieve the last booleen value of a specific option name
     *
     *  This is the boolean variant of @GetLastValue()
     *
     * @param k    std::string containing the option name to look-up
     * @return  Returns a bool with the collected value
     */
    bool GetLastBoolValue(const std::string& k)
    {
        return parse_bool_value(k, key_value.at(k).back());
    }


    /**
     *  Retrieve all parsed values for a specific option name.
     *
     * @param k    std::string containing the option name to look-up
     * @return Returns a std::vector<std::string> containing the list of
     *         values affiliated with the provided option name.
     */
    std::vector<std::string> GetAllValues(const std::string k) const
    {
        try
        {
            return key_value.at(k);
        }
        catch (const std::out_of_range&)
        {
            return {};
        }
    }


    /**
     *  Some arguments are not invalid but will be parsed as extra/additional
     *  arguments and put aside.  All these arguments are gathered separately
     *  and will be found through this call.
     *
     * @return  Returns a std::vector<std::string> list containing all the
     *          various arguments parsed and ready for furhter processing.
     */
    std::vector<std::string> GetAllExtraArgs() const
    {
        return extra_args;
    }

protected:
    std::string argv0;
    std::map<std::string, std::vector<std::string>> key_value;
    std::vector<std::string> present;
    std::vector<std::string> extra_args;
    bool completed = false;


private:
    bool parse_bool_value(const std::string& k, const std::string& value) const;
    void remove_arg(const std::string& opt);
};



/**
 *  Simplistic internal specification of callback function APIs
 */
using commandPtr = int (*)(ParsedArgs::Ptr);
using argHelperFunc = std::string (*)();


/**
 *   This is an internal class used to populate the ParsedArgs class.
 *   This is primarily called by the SimpleCommand class which will call
 *   the provided callback functions with a ParsedArgs object of the parsed
 *   options and arguments.
 */
class RegisterParsedArgs : public virtual ParsedArgs
{
public:
    typedef std::shared_ptr<RegisterParsedArgs> Ptr;

    RegisterParsedArgs(const std::string arg0) : ParsedArgs(arg0)
    {
    }

    virtual ~RegisterParsedArgs() = default;

    /**
     *  Registers an option with an optional value.  If value is NULL,
     *  it will be flagged as present only.
     *
     * @param k  std::string containing the option name
     * @param v  char * containing NULL or a pointer to the option value (string)
     *
     */
    void register_option(const std::string k, const char *v);


    /**
     *  Registers arguments provided which was not picked up by any of the
     *  configured options.
     *
     * @param e  char * containing the value to put aside
     */
    void register_extra_args(const char * e);


    /**
     *  Indicates that the command line parsing completed and the designated
     *  callback function may be run.
     */
    void set_completed();
};  // class RegisterParsedArgs



/**
 *  This class typically handles a single and specific option.
 *
 *  It essentially helps gathering and preparing the needed information
 *  both for the getopt_long() parser as well as generating useful text strings
 *  to be used in help screens.
 */
class SingleCommandOption : public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<SingleCommandOption> Ptr;

    /**
     * Registers an option, both short and long options, which does not take
     * any additional value argument.  The short option is optional and can
     * be set to 0 if no short option is required.
     *
     * @param longopt    std::string containing the long option name
     * @param shrtopt    char containing the single option character to use.
     *                   Can be 0 if no short option is required
     * @param help_text  A simple help text which describes this option in the
     *                   --help screen.
     */
    SingleCommandOption(const std::string longopt,
                        const char shrtopt,
                        const std::string help_text);


    /**
     *  Similar to the other SingleCommandOption constructor.  This one takes
     *  two additional arguments to indicate it may need a value to provided
     *  together with the option.  Through the second new argument, it can
     *  also be indicated if it is required or optional.
     *
     * @param longopt    std::string containing the long option name
     * @param shrtopt    char containing the single option character to use.
     *                   Can be 0 if no short option is required
     * @param metavar    A simple string describing the additional value; only
     *                   used by the --help screen
     * @param required   Indicates if this option is required (true) or
     *                   optional (false).
     * @param help_text  A simple help text which describes this option in the
     *                   --help screen.
     */
    SingleCommandOption(const std::string longopt,
                        const char shrtopt,
                        const std::string metavar,
                        const bool required,
                        const std::string help_text,
                        const argHelperFunc arg_helper_func = nullptr);


    ~SingleCommandOption();


    /**
     *  Sets an alias option for this option.  Only the long
     *  option type is supported.
     *
     * @param optalias  std::string with the alias to use
     */
    void SetAlias(const std::string& optalias);


    /**
     *  Returns a string containing the registered option, formatted
     *  to be used by shell completion scripts
     *
     * @return std::string containing the shell completion string for
     *         this option
     */
    std::string get_option_list_prefixed();


    /**
     *  For the shell completion of option arguments, some data is runtime
     *  depended and must be retrieved with live data.  Options arguments
     *  which provided a function pointer to a argument-helper-callback will
     *  call this function to get the needed data.
     *
     * @return Must return a string of possible values where each value is
     *         separated by space
     */
    std::string call_argument_helper_callback(const std::string& opt_name);


    /**
     *  Returns the section of a getopt_long() optstring value which
     *  represents this specific option.  It will also hint to getopt_long()
     *  whether this option may carry or expects an additional value.
     *
     * @return  Returns a std::string containing a getopt_long() compatible
     *          optstring describing this particular option.
     */
    std::string getopt_optstring();


    /**
     *  Checks if the provided short option matches the value of this
     *  object's registered short option.
     *
     * @param o  const char containing the short option to check
     * @return   Returns true if it is a match, otherwise false
     */
    bool check_short_option(const char o);


    /**
     *  Checks if the provided long option matches the value of this
     *  object's registered long or alias option.
     *
     * @param o  const char * string containing the long option to check
     * @return   Returns true if it is a match, otherwise false
     */
    bool check_long_option(const char *o);


    /**
     *  Retrieve the long option name for this option object
     *
     * @return Returns a std::string with this objects long option name.
     */
    std::string get_option_name();


    /**
     *  Returns the prepared struct option member which is prepared
     *
     * @return A pointer to a prepared struct option element
     */
    std::vector<struct option *> get_struct_option();


    /**
     *  Generates a line (or more) of information related to this specific
     *  option.  If the line gets too long, it attempts to shorten it and
     *  will generate more lines for this option.
     *
     * @param width  Unsigned int defining the available width for the
     *               option/argument part of the output.  Set to 30 by
     *               default.
     *
     * @return Returns a std::vector<std::string >containing one or more
     *         newline separated lines describing this specific option.
     */
    std::vector<std::string> gen_help_line(const unsigned int width = 30);


    std::string gen_help_line_generator(const char opt_short,
                                        const std::string& opt_long,
                                        const std::string& opt_help,
                                        const unsigned int width);

private:
    const std::string longopt;
    const char shortopt;
    const std::string metavar;
    const argHelperFunc arg_helper_func;
    const std::string help_text;
    std::string alias;
    struct option getopt_option;
    struct option getopt_alias;


    /**
     *  Used internally to just prepare a getopt_long() related struct option
     *  element.  This element is later on gathered and provided to
     *  getopt_long() before starting to parse the command line arguments.
     *
     * @param longopt    const std::string containing the long option name,
     *                   represented in the struct option .name variable.
     * @param shortopt   const char representing the short option name, which
     *                   is saved in struct option .val
     * @param has_args   const int indicating various flags in use,
     *                   represented in the .flag variable.  Used to indicate
     *                   if this option has no argument (no_argument), an
     *                   optional argument (optional_argument) or required
     *                   argument (required_argument)
     *
     */
    void update_getopt(const std::string longopt, const char  shortopt,
                       const int has_args);
};  // class SingleCommandOption



/**
 *  A SingleCommand object represents a single callback function and command
 *  name to be provided at the command line.  A SingleCommand is then built
 *  up with one or more SingleCommandOption describing the options this
 *  command support.  A single binary can also contain several commands.
 */
class SingleCommand : public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<SingleCommand> Ptr;

    /**
     *  Define a new command to the binary
     *
     * @param command      std::string of the command name you want to use
     * @param description  std::string with a short description of this command
     * @param cmdfunc      Callback function to run when this command is
     *                     invoked at the command line.
     */
    SingleCommand(const std::string command,
                  const std::string description,
                  const commandPtr cmdfunc)
        : command(command), description(description), command_func(cmdfunc),
          opt_version_added(false)
    {
        options.push_back(new SingleCommandOption("help", 'h',
                                                  "This help screen"));
    }


    /**
     *   Add an alias command to this main command.  This is useful
     *   when wanting to deprecate a command while keeping the old one
     *   still functional.
     *
     * @param alias    std::string of the alias name
     * @param remark   std::string with a message being shown when used
     */
    void SetAliasCommand(const std::string& alias,
                         const std::string& remark = {});

    /**
     *   Retrieve the alias set for this command
     *
     * @return Returns a std::string with the alias
     */
    const std::string GetAliasCommand() const;


    /**
     * Adds a new option to the current command.  This takes both a
     * long and short option without any additional value arguments.
     *
     * @param longopt    std::string of the long option name to use
     * @param shortopt   char containing the short option name to use
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     *
     * @return Returns SingleCommandOption::Ptr belonging to the new option
     *
     */
    SingleCommandOption::Ptr AddOption(const std::string longopt,
                                       const char shortopt,
                                       const std::string help_text);


    /**
     * Adds a new option to the current command.  This takes both a
     * long and short option and  also indicates this option takes an
     * additional value argument and indicates if this value is required or
     * optional.
     *
     * @param longopt    std::string of the long option name to use
     * @param shortopt   char containing the short option name to use
     * @param metavar    std::string containing a short description of this
     *                   additional option value
     * @param required   If true, this additional value is required.  Otherwise
     *                   it is optional if false.
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     *
     * @return Returns SingleCommandOption::Ptr belonging to the new option
     *
     */
    SingleCommandOption::Ptr AddOption(const std::string longopt,
                                       const char shortopt,
                                       const std::string metavar,
                                       const bool required,
                                       const std::string help_text,
                                       const argHelperFunc arg_helper = nullptr);


    /**
     * Adds a new option to the current command.  This takes only a
     * long option without any additional value arguments.
     *
     * @param longopt    std::string of the long option name to use
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     *
     * @return Returns SingleCommandOption::Ptr belonging to the new option
     *
     */
    SingleCommandOption::Ptr AddOption(const std::string longopt,
                   const std::string help_text)
    {
        return AddOption(longopt, 0, help_text);
    }


    /**
     * Adds a new option to the current command.  This takes only a
     * long option.  This also indicates this option takes an additional
     * value argument and indicates if this value is required or optional
     *
     * @param longopt    std::string of the long option name to use
     * @param metavar    std::string containing a short description of this
     *                   additional option value
     * @param required   If true, this additional value is required.  Otherwise
     *                   it is optional if false.
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     *
     * @return Returns SingleCommandOption::Ptr belonging to the new option
     *
     */
    SingleCommandOption::Ptr AddOption(const std::string longopt,
                                       const std::string metavar,
                                       const bool required,
                                       const std::string help_text,
                                       const argHelperFunc arg_helper = nullptr)
    {
        return AddOption(longopt, 0, metavar, required, help_text, arg_helper);
    }


    /**
     *  Adds a default --version option, which will be handled internally
     *  by this class
     *
     */
    void AddVersionOption(const char shortopt = 0);


    /**
     *  Retrieve the basic information for the initial --help screen.
     *  This is only used when calling -h/--help/help on the initial binary
     *  without providing any command to it.
     *
     * @param width  unsigned int of the width to consider for the
     *               binary/command line.  By default, it is set to 20.
     * @return
     */
    std::string GetCommandHelp(unsigned int width=20);


    /**
     *  Generate a list of all options for this command.  This is used
     *  by shell completion scripts to provide hints to the user with
     *  possible options.
     *
     * @return Returns a string with all registered options, prefixed with
     *         '-' or '--' and each option is separated by space.
     */
    std::string GetOptionsList();


    /**
     *  Request calling the argument helper function for a specific option.
     *  This is used by shell completion scripts to provide value hints for a
     *  specific option.
     *
     * @param option_name  std::string containing the option to get
     *                     value hints for
     * @return  Returns a string with possible values for the provided option.
     *          Each value is separated by a space.
     */
    std::string CallArgumentHelper(const std::string option_name);


    /**
     *  Get the registered command name for this object
     *
     * @return std::string containing the command name
     */
    std::string GetCommand()
    {
        return command;
    }


    /**
     *  Check if the currently registered command name matches the one
     *  provided in the argument.
     *
     * @param cmdn   std::string of the command name to match against.
     * @return
     */
    bool CheckCommandName(const std::string cmdn)
    {
        return (cmdn == command)
               || (!alias_cmd.empty() && cmdn == alias_cmd);
    }


    /**
     *  This starts the command line parsing for this specific command.
     *  Once the command line argument parsing has succeeded the gathered
     *  information (stored in a ParsedArgs object) is sent to the callback
     *  function of this object.
     *
     *  This variant allows defining how many arguments/options should be
     *  skipped before the argument parser starts the processing.
     *
     * @param arg0   std::string containing the basic name of the current
     *               binary
     * @param skip   Number of argument elements to skip
     * @param argc   int value containing number of arguments in argv; this
     *               mimics the standard C/C++ way of argument passing
     * @param argv   char ** string array with all the privded arguments.
     *
     * @return  Returns the same exit code as the callback function returned.
     */
    virtual int RunCommand(const std::string arg0, unsigned int skip,
                           int argc, char **argv);


    /**
     *  This starts the command line parsing for this specific command.
     *  Once the command line argument parsing has succeeded the gathered
     *  information (stored in a ParsedArgs object) is sent to the callback
     *  function of this object.
     *
     * @param arg0   std::string containing the basic name of the current
     *               binary
     * @param argc   int value containing number of arguments in argv; this
     *               mimics the standard C/C++ way of argument passing
     * @param argv   char ** string array with all the privded arguments.
     *
     * @return  Returns the same exit code as the callback function returned.
     */
    virtual int RunCommand(const std::string arg0, int argc, char **argv)
    {
        return RunCommand(arg0, 0, argc, argv);
    }


protected:
    /**
     *  Parse the command line arguments the program was started with, with
     *  some minor tweaks (see ProcessCommandLine() for details)
     *
     * @param arg0  std::string containing the program name itself
     * @param argc   int value containing number of arguments in argv; this
     *               mimics the standard C/C++ way of argument passing
     * @param argv   char ** string array with all the privded arguments.
     *
     * @return Returns a ParsedArgs::Ptr with all the various parsed options
     *         accessible in a structured way.  Always check the result of
     *         the GetCompleted() method in this object.  If this does not
     *         return true, the execution should stop as the command line
     *         parsing did not complete properly; most likely due to
     *         -h or --help.
     */
    RegisterParsedArgs::Ptr parse_commandline(const std::string & arg0,
                                              unsigned int skip,
                                              int argc, char **argv);


private:
    const std::string command;
    const std::string description;
    const commandPtr command_func;
    std::string alias_cmd;
    std::string alias_remark;
    std::vector<SingleCommandOption::Ptr> options;
    std::string shortopts;
    bool opt_version_added;


    /**
     * Initialise an getopt_long() related struct option.  This is used by
     * getopt_long() to proplerly parse the command line.  This information
     * is gathered by calling the getopt_struct_option() methods in each
     * SignleCommandOption class.
     *
     * @param result  A struct option pointer to where to save all this
     * information.
     */
    struct option *init_getopt();


    /**
     *  Generates the complete help screen for this specific command.  This
     *  will typically print an introduction and then get all the help
     *  lines directly from each option class.
     *
     * @param arg0
     * @return
     */
    std::string gen_help(const std::string arg0);
};  // class SingleCommand



/**
 *  Command container.  Keeps track of all registered commands.  It
 *  also takes care of parsing the command line and pass the proper
 *  arguments to the proper registered SingleCommand object, which again
 *  will run the designated callback function
 *
 *  Whenever an error occurs, a CommandException will be thrown
 *
 */
class Commands : public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<Commands> Ptr;

    /**
     *  Instantiate the Commands container
     *
     * @param progname     A short string defining this program
     * @param description  A short description of this programs function/role
     */
    Commands(const std::string progname, const std::string description)
        : progname(progname), description(description)
    {
        // Register a new ShellCompletion helper object.  This
        // will automatically build up the 'shell-completion' command,
        // based on the commands and options being added.
        shellcompl.reset(new ShellCompletion());
        commands.push_back(shellcompl);
    }

    /**
     *  Register a new command with a reference to the callback
     *  function.
     *
     * @param cmd  SingleCommand::Ptr object pointing to the command
     *             to register to the main program.
     */
    void RegisterCommand(const SingleCommand::Ptr cmd);


    /**
     *  Starts the command line processing, which will on success run
     *  the proper callback function according to the command being called.
     *
     * @param argc
     * @param argv
     * @return
     */
    int ProcessCommandLine(int argc, char **argv);


    /**
     *  Primarily to be used by the ShellCompletion object.  It will return
     *  a std::vector with pointers to all the registered SingleCommand
     *  objects
     *
     * @return  std::vector<SingleCommand::Ptr>, which represents
     *          smart-pointers to all the registered single command objects.
     */
    std::vector<SingleCommand::Ptr> GetAllCommandObjects();


private:
    /**
     *  Subclass of Commands, which builds the shell-completion command
     *  automatically based on all the registered commands.  The output this
     *  class produces is to be used by various shell completion scripts
     */
    class ShellCompletion : public SingleCommand
    {
    public:
        typedef RCPtr<ShellCompletion> Ptr;

        ShellCompletion();

        /**
         *  Provide a "back-pointer" to the parent Commands objects.  This
         *  is needed to be able to extract all the various commands,
         *  options and the argument helper function in each of the registered
         *  commands.
         *
         * @param cmds  Commands * to the parent Commands object.
         */
        void SetMainCommands(Commands * cmds);

        /**
         *   Since this class inherits the SingleCommand class, we implement
         *   the argument parsing for the shell completion command here.
         *
         * @param arg0   std::string containing the basic name of the current
         *               binary
         * @param ignored_skip  This argument will be ignored; it is here
         *               purely for API compatibility
         * @param argc   int value containing number of arguments in argv;
         *               this mimics the standard C/C++ way of argument
         *               passing
         * @param argv   char ** string array with all the privded arguments.
         *
         * @return  Will always return 0, as we do not depend on exit codes
         *          when generating shell completion strings.
         */
        int RunCommand(const std::string arg0, unsigned int ignored_skip,
                       int argc, char **argv);

    private:
        Commands * commands;

        /**
         *  Helper command to be used by various command completion
         *  capable shells.  It lists just all the various available commands.
         *  The result is written straight to stdout.
         */
        void list_commands();

        /**
         *  Generate the list of options for a specific command.  This
         *  job is done by the SingleCommand object itself, through the
         *  SingleCommand::GetOpttionList() method.  The result is written
         *  straight to stdout.
         *
         * @param cmd  std::string containing the command to query for
         *             available options.
         */
        void list_options(const std::string cmd);

        /**
         *  The argument helper callback function generates a list of possible
         *  values to use for a specific option in a specific command.
         *  Similar to list_options(), just more specific.  When this is
         *  triggered, the output generated by the argHelperFunc function
         *  is written directly to stdout.
         *
         * @param cmd     std::string containing the command to query
         * @param option  std::string containing the option to query for
         *                possible values.
         */
        void call_arg_helper(const std::string cmd, const std::string option);
    }; // class Commands::ShellCompletion

    /**
     *   Print an initial help screen, providing an overview of all
     *   available commands in this program
     *
     * @param arg0  std::string containing the binary name (typically argv[0])
     *
     */
    void print_generic_help(std::string arg0);


    const std::string progname;
    const std::string description;
    std::vector<SingleCommand::Ptr> commands;
    ShellCompletion::Ptr shellcompl;
};  // class Commands
