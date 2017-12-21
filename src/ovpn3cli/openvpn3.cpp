//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#include <iostream>
#include <sstream>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <getopt.h>

#include <client/ovpncli.cpp>

#include "dbus/core.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"

using namespace openvpn;

using commandPtr = int (*)(std::vector<std::string>&);

struct OpenVPN_option_list {
    std::string optname;
    char shortopt;
    int  has_args;
    std::string argdescr;
    std::string help_text;
    commandPtr func;
};


/**
 *  Print a formatted help screen, based on the option lists provided
 *
 *  @param arg0    A string containing the program name
 *  @param opts    A vector of option structs, containing all supported options
 */
void help_screen(const char * arg0, std::vector<struct OpenVPN_option_list>& opts)
{
    std::vector<std::string> summary;
    std::vector<std::string> helpline;

    // Process all options
    for (auto& o: opts)
    {
        std::stringstream sopt;
        std::stringstream help;

        // Prepare the listing of short options
        if (0 != o.shortopt) {
            sopt << "-" << o.shortopt << "|";
            help << "-" << o.shortopt << " | ";
        }

        // Append the listing of long options
        sopt << "--" << o.optname;
        help << "--" << o.optname;

        // Add argument description if provided
        if( no_argument != o.has_args
            && !o.argdescr.empty())
        {
            sopt << " <" << o.argdescr << ">";
            help << " <" << o.argdescr << ">";
        }

        // Save the prepared list of option and arguments
        // to be used as part of the "Usage: progname ...opts..." line
        summary.push_back(sopt.str());

        // Prepare the more descriptive help lines
        help << std::setw(40 - help.str().length())
             << " " << o.help_text;
        helpline.push_back(help.str());
    }

    //
    //  Print the processed information to stdout
    //

    unsigned indent = 8 + strlen(arg0);
    unsigned len = indent + 12;
    std::cout << "Usage: " << arg0 << " [-h|--help] ";
    for (auto& o: summary)
    {
        len += o.length() + 3;
        if (len > 75)
        {
            std::cout << std::endl
                      << std::setw(indent)
                      << " ";
            len = indent;
        }
        std::cout << "[" << o << "] ";
   }
    std::cout << std::endl << std::endl;

    for (auto& o: helpline)
    {
        std::cout << "    " << o << std::endl;
    }
    std::cout << std::endl;
}


/**
 *  Convert a vector/list of OpenVPN_option_list to getopt's struct option lists
 *
 *  @param *result[]  Return pointer to the struct option list, to be consumed by getopt()
 *  @param **result   Return pointer to a char array of all short opts, to be consumed by getopt()
 *  @param input      Vector of struct OpenVPN_option_list, containing all the options we must accept
 *
 */
void convert_options(struct option *result[],
                     char **result_short,
                     std::vector<struct OpenVPN_option_list>&input)
{
    // Add one extra for --help|-h and one for the terminator
    *result = (struct option *) calloc(input.size() + 2, sizeof(struct option));
    *result_short = (char *) calloc((input.size() + 2) * 2, sizeof(char));

    uint8_t i = 0;
    uint8_t shortidx = 0;
    for (auto& opt: input)
    {
        (*result)[i].name = opt.optname.c_str();
        (*result)[i].has_arg = opt.has_args;
        (*result)[i].flag = NULL;
        (*result)[i].val = opt.shortopt;
        i++;

        (*result_short)[shortidx++] = opt.shortopt;
        if (no_argument != opt.has_args)
        {
            (*result_short)[shortidx++] = ':';
        }
    }
    (*result)[i].name = (char *)"help";
    (*result)[i].has_arg = no_argument;
    (*result)[i].flag = NULL;
    (*result)[i].val = 'h';
    (*result_short)[shortidx] = 'h';
}


/**
 *  Processes the command line arguments according to the list of valid options
 *  which is defined in the vector of struct OpenVPN_option_list elements
 *
 *  @param argc  Main programs argc argument
 *  @param argv  Main programs argv argument
 *  @param opts  Vector of struct OpenVPN_option_list elements containing all
                 the options the program supports
 *
 *  @return Returns a tuple containing a pointer to the function to call plus
 *          a vector of strings containing the arguments to be processed further
 *          by the function to be called.
 */
std::tuple<commandPtr, std::vector<std::string>> process_command_line(int argc, char **argv, std::vector<struct OpenVPN_option_list>&opts)
{
    // Convert the OpenVPN_option_list vector
    // to something getopt_long() understands
    struct option *ovpn_options = NULL;
    char *ovpn_shortopts = NULL;
    convert_options(&ovpn_options, &ovpn_shortopts, opts);

    std::vector<std::string> args;
    int c;
    while (1)
    {
        int optidx = 0;

        c = getopt_long(argc, argv, ovpn_shortopts, ovpn_options, &optidx);
        if (c == -1)
        {
            break;
        }

        if ('h' == c)
        {
            help_screen(argv[0], opts);
            free(ovpn_options);
            free(ovpn_shortopts);
            exit(0);
        }

        for (auto& o: opts)
        {
            if (o.shortopt == c) {
                if (NULL != optarg) {
                    args.push_back(std::string(optarg));
                }
                if (optind < argc)
                {
                    // All additional arguments gets saved for further
                    // processing inside the function to be called
                    while (optind < argc)
                    {
                        args.push_back(std::string(argv[optind++]));
                    }
                }
                free(ovpn_options);
                free(ovpn_shortopts);

                // Extract the function to be called and return it
                // together with additional arguments
                return std::make_tuple(o.func, args);
            }
        }
    }
    help_screen(argv[0], opts);
    exit(1);
}


int show_version(std::vector<std::string>& args)
{
    std::cout << get_version("/openvpn3") << std::endl;
    return 0;
}

void print_statistics(ConnectionStats& stats)
{
    if (stats.size() < 1)
    {
        return;
    }

    std::cout << std::endl << "Connection statistics:" << std::endl;
    for (auto& sd : stats)
    {
        std::cout << "     "
                  << sd.key
                  << std::setw(20-sd.key.size()) << std::setfill('.') << "."
                  << std::setw(12) << std::setfill('.')
                  << sd.value
                  << std::endl;
    }
    std::cout << std::endl;
}


/**
 *  Parses and imports an OpenVPN configuration file and saves it
 *  within the OpenVPN 3 Configuration Manager service.
 *
 *  This parser will also ensure all external files are embedded into
 *  the configuration before sent to the configuration manager.
 *
 *  @param filename    Filename of the configuration file to import
 *  @param single_use  This will make the Configuration Manager automatically
 *                     delete the configuration file from its storage upon the first
 *                     use.  This is used for load-and-connect scenarios where it
 *                     is not likely the configuration will be re-used
 *  @param persistent  This will make the Configuration Manager store the configuration
 *                     to disk, to be re-used later on.
 *
 *  @return Retuns a string containing the D-Bus object path to the configuration
 */
std::string import_config(std::string filename, bool single_use, bool persistent)
{
    // Parse the OpenVPN configuration
    // The ProfileMerge will ensure that all needed
    // files are embedded into the configuration we
    // send to and store in the Configuration Manager
    ProfileMerge pm(filename, "", "",
                    ProfileMerge::FOLLOW_FULL,
                    ProfileParseLimits::MAX_LINE_SIZE,
                    ProfileParseLimits::MAX_PROFILE_SIZE);

    // Import the configuration file
    OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration);
    return conf.Import(filename, pm.profile_content(), single_use, persistent);
}


int load_config(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Loading a configuration file requires only 1 argument"
                  << std::endl;
        return 1;
    }

    try
    {
        std::string cfgpath = import_config(args[0], false, false);
        std::cout << "Configuration imported.  Configuration path: " << cfgpath << std::endl;
        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to import configuration: " << err.getRawError() << std::endl;
        return 2;
    }
}

int connect(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Starting a new connection requires only 1 argument" << std::endl;
        return 1;
    }

    OpenVPN3SessionProxy sessionmgr(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_sessions);
    std::string sessionpath = sessionmgr.NewTunnel(args[0]);
    sleep(1);  // Allow session to be established (FIXME: Signals?)
    std::cout << "Session path: " << sessionpath << std::endl;
    OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, sessionpath);

    unsigned int loops = 10;
    while (loops > 0)
    {
        loops--;
        try
        {
            session.Ready();  // If not, an exception will be thrown
            session.Connect();

            // Allow approx 30 seconds to establish connection; one loop
            // will take about 1.3 seconds.
            unsigned int attempts = 23;
            BackendStatus s;
            while (attempts > 0)
            {
                attempts--;
                usleep(300000);  // sleep 0.3 seconds - avg setup time
                s = session.GetLastStatus();
                if (s.minor == StatusMinor::CONN_CONNECTED)
                {
                    std::cout << "Connected" << std::endl;
                    return 0;
                }
                else if (s.minor == StatusMinor::CONN_DISCONNECTED)
                {
                    attempts = 0;
                    break;
                }
                sleep(1);  // If not yet connected, wait for 1 second
            }

            if (attempts < 1)
            {
                // FIXME: Look into using exceptions here, catch more
                // fine grained connection issues from the backend
                std::cout << "Failed to connect "
                          << "[" << StatusMajor_str[(unsigned int)s.major]
                          << " / " << StatusMinor_str[(unsigned int)s.minor]
                          << "]" << std::endl;
                if (!s.message.empty())
                {
                    std::cout << s.message << std::endl;
                }
                session.Disconnect();
                return 3;
            }
        }
        catch (ReadyException& err)
        {
            // If the ReadyException is thrown, it means the backend
            // needs more from the front-end side
            for (auto& type_group : session.QueueCheckTypeGroup())
            {
                ClientAttentionType type;
                ClientAttentionGroup group;
                std::tie(type, group) = type_group;

                if (ClientAttentionType::CREDENTIALS == type)
                {
                    std::vector<struct RequiresSlot> reqslots;
                    session.QueueFetchAll(reqslots, type, group);
                    for (auto& r : reqslots)
                    {
                        std::string response;
                        if (!r.hidden_input)
                        {
                            std::cout << r.user_description << ": ";
                            std::cin >> response;
                        }
                        else
                        {
                            std::string prompt = r.user_description + ": ";
                            char *pass = getpass(prompt.c_str());
                            response = std::string(pass);
                        }
                        r.value = response;
                        session.ProvideResponse(r);
                    }
                }
            }
        }
        catch (DBusException& err)
        {
            std::cout << "Failed to start new session: " << err.getRawError() << std::endl;
            return 2;
        }
    }
    return 1;
}


int dump_stats(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Disconnecting a connection requires only 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, args[0]);
        ConnectionStats stats = session.GetConnectionStats();

        if (stats.size() == 0)
        {
            std::cout << "No connection statistics available" << std::endl;
            return 0;
        }
        print_statistics(stats);
        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to fetch statistics: " << err.getRawError() << std::endl;
        return 2;
    }
}


int disconnect(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Disconnecting a connection requires only 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, args[0]);
        ConnectionStats stats = session.GetConnectionStats();
        session.Disconnect();
        std::cout << "Initiated session shutdown." << std::endl;
        print_statistics(stats);

        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to disconnect session: " << err.getRawError() << std::endl;
        return 2;
    }
}

int pause_session(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Pausing a connection requires 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, args[0]);
        session.Pause("Front-end request");
        std::cout << "Initiated session pause: " << args[0] << std::endl;

        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to pause session: " << err.getRawError() << std::endl;
        return 2;
    }
}

int resume_session(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Pausing a connection requires 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, args[0]);
        session.Resume();
        std::cout << "Initiated session resume: " << args[0] << std::endl;

        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to resume session: " << err.getRawError() << std::endl;
        return 2;
    }
}


int run_config(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Starting an OpenVPN configuration file requires 1 argument" << std::endl;
        return 1;
    }

    try
    {
        std::string cfgpath = import_config(args[0], true, false);
        std::vector<std::string> sessargs;
        sessargs.push_back(cfgpath);
        connect(sessargs);

        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to import configuration: " << err.getRawError() << std::endl;
        return 2;
    }
}


int set_alias(std::vector<std::string>& args)
{
    if (args.size() != 2)
    {
        std::cout << "** ERROR **  Setting aliases requires only 2 arguments" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, args[0]);
        conf.SetAlias(args[1]);
        std::cout << "Alias set to '" << args[1] << "' "
                  << "for configuration path '" << args[0] << "'" << std::endl;
        return 0;
    }
    catch (DBusPropertyException& err)
    {
        std::cout << "** ERROR ** " << err.getRawError() << std::endl;
        return 2;
    }
    catch (DBusException& err)
    {
        std::cout << "** ERROR ** " << err.getRawError() << std::endl;
        return 2;
    }
}


int remove_config(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Deleting a OpenVPN configuration file requires 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, args[0]);
        conf.Remove();
        std::cout << "Configuration removed." << std::endl;
        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to delete configuration: " << err.getRawError() << std::endl;
        return 2;
    }

}


int seal_config(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Sealing a OpenVPN configuration file requires 1 argument" << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, args[0]);
        conf.Seal();
        std::cout << "Configuration sealed." << std::endl;
        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "Failed to seal configuration: " << err.getRawError() << std::endl;
        return 2;
    }

}

int list_configs(std::vector<std::string>& args)
{
    return 3;
}

int show_config(std::vector<std::string>& args)
{
    if (args.size() != 1)
    {
        std::cout << "** ERROR **  Showing a configuration requires only 1 argument"
                  << std::endl;
        return 1;
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, args[0]);

        std::cout << "Configuration: " << std::endl
                  << "  - Name:       " << conf.GetStringProperty("name") << std::endl
                  << "  - Read only:  " << (conf.GetBoolProperty("readonly") ? "Yes" : "No") << std::endl
                  << "  - Persistent: " << (conf.GetBoolProperty("persistent") ? "Yes" : "No") << std::endl
                  << "--------------------------------------------------" << std::endl
                  << conf.GetConfig() << std::endl
                  << "--------------------------------------------------" << std::endl;
        return 0;
    }
    catch (DBusException& err)
    {
        std::cout << "** ERROR ** " << err.getRawError() << std::endl;
        return 2;
    }

    return 3;
}


int main(int argc, char **argv)
{
    std::vector<struct OpenVPN_option_list> ovpn_options;
    ovpn_options.push_back({"version",      'V',
                no_argument, "",
                "Show version information",
                show_version});

    ovpn_options.push_back({"config",      'c',
                required_argument, "config file|alias",
                "Connect to a VPN server with provided configuration file",
                run_config});

    ovpn_options.push_back({"import-config", 'i',
                required_argument, "config file",
                "Imports a configuration to the configuration manager",
                load_config});

    ovpn_options.push_back({"alias",       'a',
                required_argument, "config path> <alias",
                "Sets an alias name for a loaded configuration",
                set_alias});

    ovpn_options.push_back({"seal",       'S',
                required_argument, "config path",
                "Seals a configuration against further modifications",
                seal_config});

    ovpn_options.push_back({"remove-config",       'X',
                required_argument, "config path",
                "Deletes a configuration from the configuration manager",
                remove_config});

    ovpn_options.push_back({"connect",     'r',
                required_argument, "config path|alias",
                "Connect to a preloaded configuration",
                connect});

    ovpn_options.push_back({"stats",     'T',
                required_argument, "session path",
                "Shows current connection statistics",
                dump_stats});

    ovpn_options.push_back({"pause",     'P',
                required_argument, "session path",
                "Pauses an on-going session",
                pause_session});

    ovpn_options.push_back({"resume",     'R',
                required_argument, "session path",
                "Resumes a paused session",
                resume_session});

    ovpn_options.push_back({"disconnect",     'D',
                required_argument, "session path",
                "Disconnects a on-going session",
                disconnect});

    ovpn_options.push_back({"list",        'l',
                no_argument,       "",
                "Lists all loaded configurations",
                list_configs
                });

    ovpn_options.push_back({"show-config", 's',
                required_argument, "config path|alias",
                "Displays a configuration file with meta data",
                show_config});

    // Parse the command line and retrieve which command to run
    commandPtr command;
    std::vector<std::string> args;
    std::tie(command, args) = process_command_line(argc, argv, ovpn_options);

    int ret = command(args);

    return ret;
}
