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
 * @file   logservice1.cpp
 *
 * @brief  Test program for the log service and its proxy interface
 */

#include <iostream>

#include "common/cmdargparser.hpp"
#include "dbus/core.hpp"
#include "dbus/signals.hpp"
#include "log/dbus-log.hpp"
#include "log/proxy-log.hpp"


int cmd_props(ParsedArgs::Ptr args)
{
    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();
    LogServiceProxy logsrvprx(dbus.GetConnection());

    if (args->Present("log-level"))
    {
        logsrvprx.SetLogLevel(std::atoi(args->GetValue("log-level", 0).c_str()));
    }

    if (args->Present("timestamp"))
    {
        logsrvprx.SetTimestampFlag(args->GetBoolValue("timestamp", 0));
    }

    if (args->Present("dbus-details"))
    {
        logsrvprx.SetDBusDetailsLogging(args->GetBoolValue("dbus-details", 0));
    }

    std::cout << "Number of attached loggers: " << logsrvprx.GetNumAttached() << std::endl;
    std::cout << "Log level: " << logsrvprx.GetLogLevel() << std::endl;
    std::cout << "Timestamp enabled? " << (logsrvprx.GetTimestampFlag() ? "yes" : "no") << std::endl;
    std::cout << "D-Bus details logging enabled? " << (logsrvprx.GetDBusDetailsLogging() ? "yes" : "no") << std::endl;

    return 0;
}


int cmd_send(ParsedArgs::Ptr args)
{
    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    if (!args->Present("group"))
    {
        throw CommandException("send", "Missing required --group");
    }
    unsigned int lgrp = std::atoi(args->GetValue("group", 0).c_str());

    if (!args->Present("category"))
    {
        throw CommandException("send", "Missing required --category");
    }
    unsigned int lctg = std::atoi(args->GetValue("category", 0).c_str());

    std::string path = (args->Present("object-path") ? args->GetValue("object-path", 0) : "/net/openvpn/v3/logtest");
    std::string intf = (args->Present("interface") ? args->GetValue("interface", 0) : "net.openvpn.v3.logtest");

    auto extra = args->GetAllExtraArgs();
    std::string msg = "(empty message)";
    if (extra.size() > 0)
    {
        msg = "";
        for (const auto& t : extra)
        {
            msg += t + " ";
        }
    }
    std::cout << "     Path: " << path << std::endl;
    std::cout << "Interface: " << intf << std::endl;
    std::cout << "Log event: " << msg << std::endl;
    try
    {
        LogServiceProxy logsrvprx(dbus.GetConnection());
        if (args->Present("attach"))
        {
            std::cout << "Attatching log" << std::endl;
            logsrvprx.Attach(intf);
            sleep(1); // Wait for the logger to settle this new attachment
        }

        LogSender sig(dbus.GetConnection(), (LogGroup) lgrp, intf, path);
        sig.Send("Log", g_variant_new("(uus)", lgrp, lctg, msg.c_str()));
        std::cout << "Log signal sent" << std::endl;

        if (args->Present("attach"))
        {
            sleep(1); // Wait for the logger to process the Log signal
            std::cout << "Detaching log" << std::endl;
            logsrvprx.Detach(intf);
        }

    }
    catch (std::exception& err)
    {
        std::cerr << err.what() << std::endl;
        return 1;
    }
    return 0;

}


int main(int argc, char **argv)
{
    Commands cmds("openvpn3-service-logger service tester",
                  "Simple program to test the interfaces exposted by "
                  "openvpn3-service-logger (net.openvpn.v3.log)");

    SingleCommand::Ptr props;
    props.reset(new SingleCommand("props", "Gets and sets properties",
                                  cmd_props));
    props->AddOption("log-level", 'l', "LEVEL", true,
                     "Sets the log verbosity");
    props->AddOption("timestamp", 't', "BOOLEAN", true,
                     "Sets the timestamp flag for log events. Valid values: true, false");
    props->AddOption("dbus-details", 'D', "BOOLEAN", true,
                     "Sets the D-Bus details logging flag for log events");
    cmds.RegisterCommand(props);

    SingleCommand::Ptr send;
    send.reset(new SingleCommand("send", "Sends log events", cmd_send));
    send->AddOption("attach", 'a', "Do an Attach() method call before sending log event");
    send->AddOption("object-path", 'o', "PATH", true,
                    "D-Bus path to use as the signal origin (default: /net/openvpn/v3/logtest)");
    send->AddOption("interface", 'i', "STRING", true,
                    "Interface string to use when sending log events (default: net.openvpn.v3.logtest");
    send->AddOption("group", 'g', "INTEGER", true,
                    "LogGroup value to use for the log event");
    send->AddOption("category", 'c', "INTEGER", true,
                    "LogCategory value to use for the log event");
    cmds.RegisterCommand(send);

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
        return 9;
    }
}


