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
 * @file   cmdargparser.hpp
 *
 * @brief  Command line argument parser for C++.  Built around getopt_long()
 */

#ifndef OPENVPN3_CMDARGPARSER_HPP
#define OPENVPN3_CMDARGPARSER_HPP

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

#include <openvpn/common/rc.hpp>
using namespace openvpn;

#include "common/utils.hpp"


/**
 *  Basic exception class which is thrown whenever any of the command parsing
 *  and related objects have issues.
 */
class CommandArgBaseException : public std::exception
{
public:
    /**
     *  Base exception class which only needs a message to present to the
     *  user
     *
     * @param msg      std::string containing the message to present to the user
     */
    CommandArgBaseException(const std::string msg) noexcept
        : message(msg)
    {
    }


    virtual ~CommandArgBaseException()
    {
    }


    /**
     *  Returns only the error message, if provided.  Otherwise an empty
     *  string is returned.
     *
     * @return  Returns a const char * containing the message
     */
    virtual const char * what() const noexcept
    {
        return message.c_str();
    }


    /**
     *  Gives a hint if an error message was provided or not
     *
     * @return Returns a bool value.  True if calling what() will yield
     *         more information.  Otherwise false.
     */
    virtual bool gotErrorMessage()
    {
        return !message.empty();
    }


protected:
    const std::string message;
};


/**
 *  Exception class which is thrown whenever any of the command parsing
 *  and related objects have issues.  This is expected to be thrown when there
 *  is an issue with a single command within the main program.
 */

class CommandException : public CommandArgBaseException
{
public:
    /**
     *  Most simple exception, only indicates the command which failed.
     *  This is used if an error message is strictly not needed, often
     *  printed to the console or log right before this event occurres.
     *
     * @param command  std::string containing the name of the current command
     */
    CommandException(const std::string command) noexcept
        : CommandArgBaseException(""),
          command(command)
    {
    }


    /**
     *  Similar to the simpler CommandException class, but this one
     *  allows adding a simple message providing more details about the
     *  failure.
     *
     * @param command  std::string containing the name of the current command
     * @param msg      std::string containing the message to present to the user
     */
    CommandException(const std::string command,
                     const std::string msg) noexcept
        : CommandArgBaseException(msg),
          command(command)
      {
      }


    /**
     *  Retrieves the command name where this issue occurred.  This is always
     *  available in this type of exceptions.
     *
     * @return Returns a const char * containing the name of the command
     */
    virtual const char * getCommand() const noexcept
    {
        return command.c_str();
    }


private:
    const std::string command;
};


/**
 *  Exception class which is thrown whenever any of the command parsing
 *  and related objects have issues with a specific option.
 */
class OptionException : public CommandArgBaseException
{
public:
    /**
     *  Most simple exception, only indicates the option which failed.
     *  This is used if an error message is strictly not needed, often
     *  printed to the console or log right before this event occurres.
     *
     * @param option  std::string containing the name of the current option
     */
    OptionException(const std::string option) noexcept
        : CommandArgBaseException("--" + option),
          option(option)
    {
    }


    /**
     *  Similar to the simpler OptionException class, but this one
     *  allows adding a simple message providing more details about the
     *  failure.
     *
     * @param option  std::string containing the name of the current option
     * @param msg     std::string containing the message to present to the user
     */
    OptionException(const std::string option, const std::string msg) noexcept
                    : CommandArgBaseException("--" + option + ": " + msg),
                      option(option)
    {
    }


    virtual ~OptionException()
    {
    }


    virtual const char * what() const noexcept
    {
        return message.c_str();
    }

    /**
     *  Retrieves the option name where this issue occured.  This is always
     *  available in this type of exceptions.
     *
     * @return Returns a const char * containing the name of the option
     */
    virtual const char * getOption() const noexcept
    {
        return option.c_str();
    }


private:
    const std::string option;
};


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
    ParsedArgs(const std::string argv0)
        : argv0(argv0)
    {
    }


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
     *  Get the program name (argv[0])
     * @return
     */
    std::string GetArgv0() const
    {
        return argv0;
    }
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
    unsigned int GetValueLen(const std::string k) const
    {
        return key_value.at(k).size();
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
     *  Retrieve a specific boolean option, based on option name and value
     *  index
     *
     * @param k    std::string containing the option name to look-up
     * @param idx  unsigned int of the value element to retrieve
     * @return  Returns a bool with the collected value
     */
    bool GetBoolValue(const std::string k, const unsigned int idx) const
    {
        std::string value = key_value.at(k).at(idx);
        if (("false" != value) && ("true" != value ) && (value != "no") && (value != "yes"))
        {
            throw OptionException(k, "Boolean options must be either 'false' or 'true'");
        }
        return "true" == value || "yes" == value;
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
        return key_value.at(k);
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
};

/**
 *  Simplistic internal specification of callback function APIs
 */
using commandPtr = int (*)(ParsedArgs);
using argHelperFunc = std::string (*)();


/**
 *   This is an internal class used to populate the ParsedArgs class.
 *   This is primarily called by the SimpleCommand class which will call
 *   the provided callback functions with a ParsedArgs object of the parsed
 *   options and arguments.
 */
class RegisterParsedArgs : public ParsedArgs
{
public:
    RegisterParsedArgs(const std::string arg0) : ParsedArgs(arg0)
    {
    }


    /**
     *  Registers an option with an optional value.  If value is NULL,
     *  it will be flagged as present only.
     *
     * @param k  std::string containing the option name
     * @param v  char * containing NULL or a pointer to the option value (string)
     *
     */
    void register_option(const std::string k, const char *v)
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


    /**
     *  Registers arguments provided which was not picked up by any of the
     *  configured options.
     *
     * @param e  char * containing the value to put aside
     */
    void register_extra_args(const char * e)
    {
        extra_args.push_back(std::string(e));
    }


    /**
     *  Indicates that the command line parsing completed and the designated
     *  callback function may be run.
     */
    void set_completed()
    {
        completed = true;
    }
};


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
                        const std::string help_text)
        : longopt(longopt), shortopt(shrtopt),
          metavar(""), arg_helper_func(nullptr),
          help_text(help_text)
    {
        update_getopt(longopt, shortopt, no_argument);
    }


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
                        const argHelperFunc arg_helper_func = nullptr)
        : longopt(longopt), shortopt(shrtopt),
          metavar(metavar),  arg_helper_func(arg_helper_func),
          help_text(help_text)
    {
        update_getopt(longopt, shortopt,
                      (required ? required_argument : optional_argument));
    }


    ~SingleCommandOption()
    {
        // We need to override this, as we're allocating this
        // const char * struct member dynamically while
        // getopt typically expects this to be static.  This
        // is allocated in init_getopt()
        free((char *)getopt_option.name);
    }

    /**
     *  Returns a string containing the registered option, formatted
     *  to be used by shell completion scripts
     *
     * @return std::string containing the shell completion string for
     *         this option
     */
    std::string get_option_list_prefixed()
    {
        std::stringstream r;

        if (0 != shortopt)
        {
            r << "-" << shortopt;

        }
        if (!longopt.empty())
        {
            if (0 != shortopt)
            {
                r << " ";
            }
            r << "--" << longopt;
        }
        return r.str();
    }


    /**
     *  For the shell completion of option arguments, some data is runtime
     *  depended and must be retrieved with live data.  Options arguments
     *  which provided a function pointer to a argument-helper-callback will
     *  call this function to get the needed data.
     *
     * @return Must return a string of possible values where each value is
     *         separated by space
     */
    std::string call_argument_helper_callback()
    {
            return (nullptr != arg_helper_func ? arg_helper_func() : "");
    }


    /**
     *  Returns the section of a getopt_long() optstring value which
     *  represents this specific option.  It will also hint to getopt_long()
     *  whether this option may carry or expects an additional value.
     *
     * @return  Returns a std::string containing a getopt_long() compatible
     *          optstring describing this particular option.
     */
    std::string getopt_optstring()
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


    /**
     *  Checks if the provided short option matches the value of this
     *  object's registered short option.
     *
     * @param o  const char containing the short option to check
     * @return   Returns true if it is a match, otherwise false
     */
    bool check_short_option(const char o)
    {
        return o == shortopt;
    }


    /**
     *  Checks if the provided long option matches the value of this
     *  object's registered long option.
     *
     * @param o  const char * string containing the long option to check
     * @return   Returns true if it is a match, otherwise false
     */
    bool check_long_option(const char *o)
    {
        if (NULL != o)
        {
            return std::string(o) == longopt;
        }
        return false;
    }


    /**
     *  Retrieve the long option name for this option object
     *
     * @return Returns a std::string with this objects long option name.
     */
    std::string get_option_name()
    {
        return longopt;
    }


    /**
     *  Returns the prepared struct option member which is prepared
     *
     * @return A pointer to a prepared struct option element
     */
    struct option *get_struct_option()
    {
        return &getopt_option;
    }


    /**
     *  Generates a line (or more) of information related to this specific
     *  option.  If the line gets too long, it attempts to shorten it and
     *  will generate more lines for this option.
     *
     * @param width  Unsigned int defining the available width for the
     *               option/argument part of the output.  Set to 30 by
     *               default.
     * @return Returns a std::string containing one or more newline separated
     *         lines describing this specific option.
     */
    std::string gen_help_line(const unsigned int width = 30)
    {
        std::stringstream r;

        // If we don't have a short option, fill out with blanks to
        // have the long options aligned under each other
        if (0 == shortopt)
        {
            r << "     ";
        }
        else  // ... we have a short option, format the output of it
        {
            r << "-" << shortopt << " | ";
        }
        r << "--" << longopt;

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
                r << " [" << metavar << "]";
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
        r << " - " << help_text;

        return r.str();
    }

private:
    const std::string longopt;
    const char shortopt;
    const std::string metavar;
    const argHelperFunc arg_helper_func;
    const std::string help_text;
    struct option getopt_option;


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
                       const int has_args)
    {
        // We need a strdup() as the pointer longopt.c_str() returns on
        // some systems gets destroyed - rendering garbage in this
        // struct.  In the destructor, this is free()d again.
        getopt_option.name = strdup(longopt.c_str());
        getopt_option.flag = NULL;
        getopt_option.has_arg =  has_args;
        getopt_option.val = shortopt;
    }
};


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
     * Adds a new option to the current command.  This takes both a
     * long and short option without any additional value arguments.
     *
     * @param longopt    std::string of the long option name to use
     * @param shortopt   char containing the short option name to use
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     */
    void AddOption(const std::string longopt,
                   const char shortopt,
                   const std::string help_text)
    {
        SingleCommandOption::Ptr opt = new SingleCommandOption(longopt,
                                                               shortopt,
                                                               help_text);
        options.push_back(opt);
    }


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
     */
    void AddOption(const std::string longopt,
                   const char shortopt,
                   const std::string metavar,
                   const bool required,
                   const std::string help_text,
                   const argHelperFunc arg_helper = nullptr)
    {
        SingleCommandOption::Ptr opt = new SingleCommandOption(longopt,
                                                               shortopt,
                                                               metavar,
                                                               required,
                                                               help_text,
                                                               arg_helper);
        options.push_back(opt);
    }


    /**
     * Adds a new option to the current command.  This takes only a
     * long option without any additional value arguments.
     *
     * @param longopt    std::string of the long option name to use
     * @param help_text  std::string containing the help text used on the
     *                   --help screen for this command
     */
    void AddOption(const std::string longopt,
                   const std::string help_text)
    {
        AddOption(longopt, 0, help_text);
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
     */

    void AddOption(const std::string longopt,
                   const std::string metavar,
                   const bool required,
                   const std::string help_text,
                   const argHelperFunc arg_helper = nullptr
                   )
    {
        AddOption(longopt, 0, metavar, required, help_text, arg_helper);
    }


    /**
     *  Adds a default --version option, which will be handled internally
     *  by this class
     *
     */
    void AddVersionOption(const char shortopt = 0)
    {
        AddOption("version", shortopt, "Show version information");
        opt_version_added = true;
    }


    /**
     *  Retrieve the basic information for the initial --help screen.
     *  This is only used when calling -h/--help/help on the initial binary
     *  without providing any command to it.
     *
     * @param width  unsigned int of the width to consider for the
     *               binary/command line.  By default, it is set to 20.
     * @return
     */
    std::string GetCommandHelp(unsigned int width=20)
    {
        std::stringstream ret;
        ret << command << std::setw(width - command.size())
            << " - " << description << std::endl;
        return ret.str();
    }


    /**
     *  Generate a list of all options for this command.  This is used
     *  by shell completion scripts to provide hints to the user with
     *  possible options.
     *
     * @return Returns a string with all registered options, prefixed with
     *         '-' or '--' and each option is separated by space.
     */
    std::string GetOptionsList()
    {
        std::stringstream r;
        for (auto const& opt : options)
        {
            r << opt->get_option_list_prefixed() << " ";
        }
        return r.str();
    }


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
    std::string CallArgumentHelper(const std::string option_name)
    {
        for (auto const& opt : options)
        {
            if (1 == option_name.size()
                && opt->check_short_option(option_name[0]))
            {
                return opt->call_argument_helper_callback();
            }
            else if (opt->check_long_option(option_name.c_str()))
            {
                return opt->call_argument_helper_callback();
            }
        }
        return "";
    }


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
        return cmdn == command;
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
                           int argc, char **argv)
    {
        try {
            ParsedArgs cmd_args = parse_commandline(arg0, skip, argc, argv);

            // Run the callback function.
            return cmd_args.GetCompleted() ? command_func(cmd_args) : 0;
        }
        catch (...)
        {
            throw;
        }
    }


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
     * @return Returns a ParsedArgs object with all the various parsed options
     *         accessible in a structured way.  Always check the result of
     *         the GetCompleted() method in this object.  If this does not
     *         return true, the execution should stop as the command line
     *         parsing did not complete properly; most likely due to
     *         -h or --help.
     */
    RegisterParsedArgs parse_commandline(const std::string & arg0, unsigned int skip,
                                 int argc, char **argv)
    {
        struct option *long_opts = init_getopt();

        RegisterParsedArgs cmd_args(arg0);
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
                                cmd_args.register_option(o->get_option_name(), optarg);
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
                            cmd_args.register_option(o->get_option_name(), optarg);
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
                    cmd_args.register_extra_args(argv[optind++]);
                }
            }
            cmd_args.set_completed();
        }
        catch (...)
        {
            throw;
        }
    exit:
        free(long_opts); long_opts = nullptr;
        return cmd_args;
    }


private:
    const std::string command;
    const std::string description;
    const commandPtr command_func;
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
    struct option *init_getopt()
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
            memcpy(&result[idx], opt->get_struct_option(), sizeof(struct option));
            idx++;

            // Collect all we need for the short flags too
            shortopts += opt->getopt_optstring();
        }

        // getopt_long() expects the last record in the struct option array
        // to be an empty/zeroed struct option element
        result[idx] = {0};
        return result;
    }


    /**
     *  Generates the complete help screen for this specific command.  This
     *  will typically print an introduction and then get all the help
     *  lines directly from each option class.
     *
     * @param arg0
     * @return
     */
    std::string gen_help(const std::string arg0)
    {
        std::stringstream r;
        r << arg0 << ": " << command << " - " << description << std::endl;
        r << std::endl;

        for (auto& opt : options)
        {
            r << "   " << opt->gen_help_line() << std::endl;
        }
        return r.str();
    }
};


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
     *  function
     *
     * @param cmdnam     Command line name of the command being registered
     * @param descr      A short description of the command's purpose
     * @param cmdfunc    commandPtr (function pointer) to the callback function
     *
     * @return Retuns an RC-smart pointer to the registered command object,
     *         which is used to further add options and arguments for that
     *         specific command.
     */
    SingleCommand::Ptr AddCommand(const std::string cmdnam,
                                  const std::string descr,
                                  const commandPtr cmdfunc)
    {
        SingleCommand::Ptr sc = new SingleCommand(cmdnam, descr, cmdfunc);
        commands.push_back(sc);
        return sc;
    }


    /**
     *  Starts the command line processing, which will on success run
     *  the proper callback function according to the command being called.
     *
     * @param argc
     * @param argv
     * @return
     */
    int ProcessCommandLine(int argc, char **argv)
    {
        std::string baseprog = simple_basename(argv[0]);

        if (2 > argc)
        {
            std::cout << "Missing command.  See '"
                      << baseprog <<" help' for details"
                      << std::endl;
            return 1;
        }

        // Implicit declaration of the generic help screen.
        std::string cmd(argv[1]);
        if ("help" == cmd|| "--help" == cmd || "-h" == cmd)
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
                    throw CommandException(baseprog, "Could not allocate memory for argument parsing");
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

        std::cout << argv[0] << ": Unknown command '" << cmd << "'"
                  << std::endl;
        return 1;
    }


    /**
     *  Primarily to be used by the ShellCompletion object.  It will return
     *  a std::vector with pointers to all the registered SingleCommand
     *  objects
     *
     * @return  std::vector<SingleCommand::Ptr>, which represents
     *          smart-pointers to all the registered single command objects.
     */
    std::vector<SingleCommand::Ptr> GetAllCommandObjects()
    {
        return commands;
    }


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

        ShellCompletion()
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
        void SetMainCommands(Commands * cmds)
        {
            commands = cmds;
        }


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
                       int argc, char **argv)
        {
            try {
                ParsedArgs args = parse_commandline(arg0, 1, argc, argv);

                if (!args.GetCompleted())
                {
                    return 0;
                }

                if (args.Present("list-commands")
                    && args.Present("list-options"))
                {
                    throw CommandException("shell-completion",
                                           "Cannot combine --list-commands and --list-options");
                }

                if (args.Present("list-options") && args.GetValueLen("list-options") > 1)
                {
                    throw CommandException("shell-completion",
                                           "Can only use --list-options once");
                }


                if (args.Present("arg-helper")
                    && !args.Present("list-options"))
                {
                    throw CommandException("shell-completion",
                                           "--arg-helper requires --list-options");
                }

                if (args.Present("list-commands"))
                {
                    list_commands();
                    return 0;
                }

                if (args.Present("list-options"))
                {
                    if (!args.Present("arg-helper"))
                    {
                        list_options(args.GetValue("list-options", 0));
                    }
                    else
                    {
                        call_arg_helper(args.GetValue("list-options", 0),
                                        args.GetValue("arg-helper", 0));
                    }
                }
            }
            catch (...)
            {
                throw;
            }
            return 3;
        }


    private:
        Commands * commands;

        /**
         *  Helper command to be used by various command completion
         *  capable shells.  It lists just all the various available commands.
         *  The result is written straight to stdout.
         */
        void list_commands()
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
        void list_options(const std::string cmd)
        {
            for (auto const& c : commands->GetAllCommandObjects())
            {
                if (c->GetCommand() == cmd )
                {
                    std::cout << c->GetOptionsList() << std::endl;
                    return;
                }
            }
        }


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
        void call_arg_helper(const std::string cmd, const std::string option)
        {
            for (auto const& c : commands->GetAllCommandObjects())
            {
                if (c->GetCommand() == cmd )
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
    };

    /**
     *   Print an initial help screen, providing an overview of all
     *   available commands in this program
     *
     * @param arg0  std::string containing the binary name (typically argv[0])
     *
     */
    void print_generic_help(std::string arg0)
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


    const std::string progname;
    const std::string description;
    std::vector<SingleCommand::Ptr> commands;
    ShellCompletion::Ptr shellcompl;
};

#endif // OPENVPN3_CMDARGPARSER_HPP
