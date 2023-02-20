//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log-service.cpp
 *
 * @brief  Command for managing the openvpn3-service-logger
 */


#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "common/cmdargparser.hpp"
#include "log/service-configfile.hpp"
#include "log/proxy-log.hpp"
#include "../arghelpers.hpp"


static int manage_config_file(ParsedArgs::Ptr args,
                              std::string cfgmode)
{
    //
    // Options for managing the log-service configuration file
    //
    try
    {
        std::string config_file{};
        LogServiceConfigFile config;

        try
        {
            if (!args->Present("config-file-override"))
            {
                DBus dbus(G_BUS_TYPE_SYSTEM);
                dbus.Connect();
                LogServiceProxy prx(dbus.GetConnection());
                config_file = prx.GetConfigFile();
            }
            else
            {
                config_file = args->GetLastValue("config-file-override");
            }
            std::cout << "Loading configuration file: " << config_file
                      << std::endl;
            config.Load(config_file);

            if ((config.IsPresent("syslog") || config.IsPresent("journald"))
                && (config.IsPresent("log-file") || config.IsPresent("colour")))
            {
                throw ExclusiveOptionError({"syslog", "journald", "log-file", "colour"});
            }
            config.CheckExclusiveOptions();
        }
        catch (const ConfigFileException &excp)
        {
            if ("config-show" == cfgmode)
            {
                std::cout << excp.what()
                          << std::endl;
                return 2;
            }
        }
        catch (const ExclusiveOptionError &err)
        {
            std::cerr << std::endl
                      << "==================================="
                      << "==================================="
                      << std::endl;
            std::cerr << "An error was found in the configuration file:"
                      << std::endl;
            std::cerr << " *** " << err.what() << std::endl;
            std::cerr << "==================================="
                      << "==================================="
                      << std::endl
                      << std::endl;
        }

        if ("config-show" == cfgmode)
        {
            if (!config.empty())
            {
                std::cout << config;
            }
            else
            {
                std::cout << "Configuration file is empty" << std::endl;
            }
        }
        else if ("config-set" == cfgmode
                 || "config-unset" == cfgmode)
        {
            std::string optname = args->GetLastValue(cfgmode);
            std::vector<std::string> values = args->GetAllExtraArgs();
            std::string new_value{""};

            // Simple sanity to retrieve the new config value
            if ("config-set" == cfgmode && values.size() == 0)
            {
                throw CommandException("log-service",
                                       "A value must be given to --"
                                           + cfgmode + " " + optname);
            }
            else if ("config-unset" == cfgmode && values.size() > 0)
            {
                throw CommandException("log-service",
                                       "No value can be given to --"
                                           + cfgmode + " " + optname);
            }
            else if (values.size() > 1)
            {
                throw CommandException("log-service",
                                       "Only a single value can be given to --"
                                           + cfgmode + " " + optname);
            }
            else if (values.size() == 1)
            {
                new_value = values.at(0);
            }

            // Update the configuration and save the file.
            if ("config-unset" == cfgmode)
            {
                config.UnsetOption(optname);
            }
            else
            {
                config.SetValue(optname, new_value);
            }
            try
            {
                config.CheckExclusiveOptions();
                config.Save(config_file);
                std::cout << "Configuration file updated.  "
                          << "Changes will be activated next time "
                          << "openvpn3-service-logger restarts"
                          << std::endl;
            }
            catch (const ExclusiveOptionError &err)
            {
                std::cerr << "Configuration NOT changed due to the "
                          << "following error:" << std::endl
                          << std::endl;
                std::cerr << " *** " << err.what() << std::endl
                          << std::endl;
            }
        }
    }
    catch (const OptionNotFound &)
    {
        // Nothing to do; options not present
    }
    return 0;
}

/**
 *  openvpn3 log-service
 *
 *  This command is used to query and manage the net.openvpn.v3.log service
 *  This service is a global service responsible for all logging.  Changes
 *  here affects all logging being done by this service on all attached
 *  log subscriptions.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log_service(ParsedArgs::Ptr args)
{
    try
    {
        try
        {
            std::vector<std::string> cfgopts = {"config-show",
                                                "config-set",
                                                "config-unset"};
            std::string cfgmode = args->Present(cfgopts);
            return manage_config_file(args, cfgmode);
        }
        catch (const OptionNotFound &)
        {
            // If no config file related options were found,
            // continue with the runtime configurations
        }

        DBus dbus(G_BUS_TYPE_SYSTEM);
        dbus.Connect();
        LogServiceProxy logsrvprx(dbus.GetConnection());

        std::string old_loglev("");
        unsigned int curlev = logsrvprx.GetLogLevel();
        unsigned int newlev = curlev;
        if (args->Present("log-level"))
        {
            newlev = std::atoi(args->GetValue("log-level", 0).c_str());
            if (curlev != newlev)
            {
                std::stringstream t;
                t << "            (Was: " << curlev << ")";
                old_loglev = t.str();
                logsrvprx.SetLogLevel(newlev);
            }
        }

        std::string old_tstamp("");
        bool curtstamp = logsrvprx.GetTimestampFlag();
        bool newtstamp = curtstamp;
        if (args->Present("timestamp"))
        {
            newtstamp = args->GetBoolValue("timestamp", 0);
            if (curtstamp != newtstamp)
            {
                std::stringstream t;
                if (newtstamp)
                {
                    // simple alignment trick
                    t << " ";
                }
                t << "     (Was: "
                  << (curtstamp ? "enabled" : "disabled") << ")";
                old_tstamp = t.str();
                logsrvprx.SetTimestampFlag(newtstamp);
            }
        }

        std::string old_dbusdetails("");
        bool curdbusdetails = logsrvprx.GetDBusDetailsLogging();
        bool newdbusdetails = curdbusdetails;
        if (args->Present("dbus-details"))
        {
            newdbusdetails = args->GetBoolValue("dbus-details", 0);
            if (curdbusdetails != newdbusdetails)
            {
                std::stringstream t;
                if (newdbusdetails)
                {
                    // simple alignment trick
                    t << " ";
                }
                t << "     (Was: "
                  << (curdbusdetails ? "enabled" : "disabled") << ")";
                old_dbusdetails = t.str();
                logsrvprx.SetDBusDetailsLogging(newdbusdetails);
            }
        }

        std::string oldlogprefix("");
        bool curlogprefix = logsrvprx.GetLogTagPrepend();
        bool newlogprefix = curlogprefix;
        if (args->Present("enable-log-prefix"))
        {
            newlogprefix = args->GetBoolValue("enable-log-prefix", 0);
            if (curlogprefix != newlogprefix)
            {
                std::stringstream t;
                if (newlogprefix)
                {
                    t << " ";
                }
                t << "     (Was: "
                  << (curlogprefix ? "enabled" : "disabled") << ")";
                oldlogprefix = t.str();
                logsrvprx.SetLogTagPrepend(newlogprefix);
            }
        }

        if (args->Present("list-subscriptions"))
        {
            DBusConnectionCreds creds(dbus.GetConnection());
            LogSubscribers list = logsrvprx.GetSubscriberList();
            if (list.size() == 0)
            {
                std::cout << "No attached log subscriptions" << std::endl;
                return 0;
            }

            std::cout << "Tag" << std::setw(22) << " "
                      << "PID" << std::setw(4) << " "
                      << "Bus name" << std::setw(4) << " "
                      << "Interface" << std::setw(25) << " "
                      << "Object path" << std::endl;
            std::cout << std::setw(120) << std::setfill('-')
                      << "-" << std::endl;
            std::cout << std::setfill(' ');

            for (const auto &e : list)
            {
                std::string pid;
                try
                {
                    pid = std::to_string(creds.GetPID(e.busname));
                }
                catch (DBusException &)
                {
                    pid = "-";
                }

                std::cout << e.tag << std::setw(25 - e.tag.length()) << " "
                          << pid << std::setw(7 - pid.length()) << " "
                          << e.busname << std::setw(12 - e.busname.length()) << " "
                          << e.interface << std::setw(34 - e.interface.length()) << " "
                          << e.object_path << std::endl;
            }
            std::cout << std::setw(120) << std::setfill('-')
                      << "-" << std::endl;
        }
        else
        {
            std::string log_method = logsrvprx.GetLogMethod();
            std::cout << "                 Log method: "
                      << log_method << std::endl;
            std::cout << " Attached log subscriptions: "
                      << logsrvprx.GetNumAttached() << std::endl;
            std::cout << "             Log timestamps: "
                      << (newtstamp ? "enabled" : "disabled")
                      << old_tstamp << std::endl;
            if ("journald" == log_method)
            {
                std::cout << "     Log tag prefix enabled: "
                          << (newlogprefix ? "enabled" : "disabled")
                          << oldlogprefix << std::endl;
            }
            std::cout << "          Log D-Bus details: "
                      << (newdbusdetails ? "enabled" : "disabled")
                      << old_dbusdetails << std::endl;
            std::cout << "          Current log level: "
                      << newlev << old_loglev << std::endl;
        }
    }
    catch (DBusProxyAccessDeniedException &excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service", rawerr);
    }
    catch (DBusException &excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service",
                               rawerr.substr(rawerr.rfind(":")));
    }
    return 0;
}


static std::string arghelper_logger_config_keys()
{
    LogServiceConfigFile cfg("");
    std::stringstream r;

    // These options are already handled differently, and does not require
    // a restart of the service
    std::vector<std::string> ignore = {
        "log-level", "timestamp", "service-log-dbus-details", "no-logtag-prefix"};

    for (const auto &o : cfg.GetOptions(true))
    {
        if (std::find(ignore.begin(), ignore.end(), o) == ignore.end())
        {
            r << o << " ";
        }
    }
    return r.str();
}


SingleCommand::Ptr prepare_command_log_service()
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("log-service",
                                "Manage the OpenVPN 3 Log service",
                                cmd_log_service));
    cmd->AddOption("log-level",
                   "LOG-LEVEL",
                   true,
                   "Set the log level used by the log service.",
                   arghelper_log_levels);
    cmd->AddOption("timestamp",
                   "true/false",
                   true,
                   "Set the timestamp flag used by the log service",
                   arghelper_boolean);
    cmd->AddOption("dbus-details",
                   "true/false",
                   true,
                   "Log D-Bus sender, object path and method details of log sender",
                   arghelper_boolean);
    cmd->AddOption("enable-log-prefix",
                   "true/false",
                   true,
                   "(journald log mode only) Enable log tag prefix in log lines",
                   arghelper_boolean);
    cmd->AddOption("list-subscriptions",
                   "List all subscriptions which has attached to the log service");
    cmd->AddOption("config-show",
                   "Show the current configuration file used by log-service");
    cmd->AddOption("config-set",
                   0,
                   "CONFIG-KEY",
                   true,
                   "Sets a configuration option and saves it in the config file",
                   arghelper_logger_config_keys);
    cmd->AddOption("config-unset",
                   0,
                   "CONFIG-KEY",
                   true,
                   "Removes a configuration option and updates the config file",
                   arghelper_logger_config_keys);
    cmd->AddOption("config-file-override",
                   0,
                   "CONFIG-FILE",
                   true,
                   "Overrides the default configuration file (default file "
                   "provivded by openvpn3-service-logger)");


    return cmd;
}



//////////////////////////////////////////////////////////////////////////
