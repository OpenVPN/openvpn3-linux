//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   log-service.cpp
 *
 * @brief  Command for managing the openvpn3-service-logger
 */


#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "common/cmdargparser.hpp"
#include "log/proxy-log.hpp"
#include "../arghelpers.hpp"

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
        DBus dbus(G_BUS_TYPE_SYSTEM);
        dbus.Connect();
        LogServiceProxy logsrvprx(dbus.GetConnection());

        std::string old_loglev("");
        unsigned int curlev = logsrvprx.GetLogLevel();
        unsigned int newlev = curlev;
        if (args->Present("log-level"))
        {
            newlev = std::atoi(args->GetValue("log-level", 0).c_str());
            if ( curlev != newlev )
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
            if ( curtstamp != newtstamp)
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
            if ( curdbusdetails != newdbusdetails)
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
            std::cout <<  std::setw(120) << std::setfill('-')
                      << "-" << std::endl;
            std::cout << std::setfill(' ');

            for (const auto& e : list)
            {
                std::string pid;
                try
                {
                   pid = std::to_string(creds.GetPID(e.busname));
                }
                catch (DBusException&)
                {
                    pid = "-";
                }

                std::cout << e.tag << std::setw(25 - e.tag.length()) << " "
                          << pid << std::setw(7 - pid.length()) << " "
                          << e.busname << std::setw(12 - e.busname.length()) << " "
                          << e.interface << std::setw(34 - e.interface.length()) << " "
                          << e.object_path << std::endl;
            }
            std::cout <<  std::setw(120) << std::setfill('-')
                      << "-" << std::endl;
        }
        else
        {
            std::cout << " Attached log subscriptions: "
                      << logsrvprx.GetNumAttached() << std::endl;
            std::cout << "             Log timestamps: "
                      << (newtstamp ? "enabled" : "disabled")
                      << old_tstamp << std::endl;
            std::cout << "          Log D-Bus details: "
                      << (newdbusdetails ? "enabled" : "disabled")
                      << old_dbusdetails << std::endl;
            std::cout << "          Current log level: "
                      << newlev << old_loglev << std::endl;
        }
    }
    catch (DBusProxyAccessDeniedException& excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service", rawerr);
    }
    catch (DBusException& excp)
    {
        std::string rawerr(excp.what());
        throw CommandException("log-service",
                               rawerr.substr(rawerr.rfind(":")));
    }
    return 0;
}


SingleCommand::Ptr prepare_command_log_service()
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("log-service",
                                "Manage the OpenVPN 3 Log service",
                                cmd_log_service));
    cmd->AddOption("log-level", "LOG-LEVEL", true,
                   "Set the log level used by the log service.",
                   arghelper_log_levels);
    cmd->AddOption("timestamp", "true/false", true,
                   "Set the timestamp flag used by the log service",
                   arghelper_boolean);
    cmd->AddOption("dbus-details", "true/false", true,
                   "Log D-Bus sender, object path and method details of log sender",
                   arghelper_boolean);
    cmd->AddOption("list-subscriptions",
                   "List all subscriptions which has attached to the log service");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
