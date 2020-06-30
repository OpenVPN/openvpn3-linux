//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   cmdargparser-exceptions.hpp
 *
 * @brief  Exceptions used by the command line argument parser
 */


#pragma once

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


    virtual ~CommandArgBaseException() = default;


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


    virtual ~OptionException() = default;


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


class OptionNotFound : public CommandArgBaseException
{
public:
    OptionNotFound() noexcept
        : CommandArgBaseException("")
    {
    }

    OptionNotFound(const std::string key) noexcept
        : CommandArgBaseException("Option '" + key + "' was not found")
    {
    }
};


class OptionNotPresent : public CommandArgBaseException
{
public:
    OptionNotPresent(const std::string key) noexcept
        : CommandArgBaseException("Option '" + key + "' value is not present")
    {
    }
};


/**
 *  Exception class used by @ParsedArgs::CheckExclusiveOptions()
 *  If an option used in a non-exclusive way, this will generate an
 *  error message explaining the details.
 */
class ExclusiveOptionError : public CommandArgBaseException
{
public:
    ExclusiveOptionError(const std::string& opt,
                         const std::vector<std::string> group)
        : CommandArgBaseException(generate_error(opt, group))
    {
    }

    ExclusiveOptionError(const std::vector<std::string> group)
        : CommandArgBaseException(generate_error("", group))
    {
    }

private:
    std::string generate_error(const std::string& opt,
                               const std::vector<std::string>& group) const
    {
        std::stringstream msg;
        if (!opt.empty())
        {
            msg << "Option '" << opt << "'"
                << " cannot be combined with: ";
        }
        else
        {
            msg << "These options cannot be combined: ";
        }

        bool first = true;
        for (const auto& o : group)
        {
            if (opt == o)
            {
                continue;
            }
            if (!first)
            {
                msg << ", ";
            }
            msg << o;
            first = false;
        }
        return std::string(msg.str());
    }
};


/**
 *  Exception class used by the @ConfigFile class when parsing
 *  configuration files and mapping the content to command line
 *  arguments.
 */
class ConfigFileException : public CommandArgBaseException
{
public:
    ConfigFileException(const std::string& msg)
        : CommandArgBaseException(generate_error("", msg))
    {
    }

    ConfigFileException(const std::string& cfgfile,
                        const std::string& msg)
        : CommandArgBaseException(generate_error(cfgfile, msg))
    {
    }

private:
    std::string generate_error(const std::string& cfgfile,
                               const std::string& msg)
    {
        if (!cfgfile.empty())
        {
            return std::string("Configuration file error in "
                            + std::string(cfgfile) + ": " + msg);
        }
        else
        {
            return std::string("Configuration file setup error: ") + msg;
        }
    }
};
