//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logservice1.cpp
 *
 * @brief  Test program for the log service and its proxy interface
 */

#include <iostream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "build-config.h"
#include "common/cmdargparser.hpp"
#include "log/dbus-log.hpp"
#include "log/proxy-log.hpp"


DBus::Connection::Ptr connect_to_dbus(const bool session_bus)
{
    auto bustype = (session_bus
                        ? DBus::BusType::SESSION
                        : DBus::BusType::SYSTEM);
    return DBus::Connection::Create(bustype);
}


int cmd_props(ParsedArgs::Ptr args)
{
    auto dbuscon = connect_to_dbus(args->Present("use-session-bus"));
    LogServiceProxy logsrvprx(dbuscon);

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
    auto dbuscon = connect_to_dbus(args->Present("use-session-bus"));

    if (!args->Present("group"))
    {
        throw CommandException("send", "Missing required --group");
    }
    LogGroup lgrp = static_cast<LogGroup>(std::atoi(args->GetValue("group", 0).c_str()));

    if (!args->Present("category"))
    {
        throw CommandException("send", "Missing required --category");
    }
    LogCategory lctg = static_cast<LogCategory>(std::atoi(args->GetValue("category", 0).c_str()));

    std::string path = (args->Present("object-path") ? args->GetValue("object-path", 0) : "/net/openvpn/v3/logtest");
    std::string intf = (args->Present("interface") ? args->GetValue("interface", 0) : "net.openvpn.v3.logtest");

    auto extra = args->GetAllExtraArgs();
    std::string msg = "(empty message)";
    if (extra.size() > 0)
    {
        msg = "";
        for (const auto &t : extra)
        {
            msg += t + " ";
        }
    }

    Events::Log ev(lgrp, lctg, msg);
    std::cout << "     Path: " << path << std::endl;
    std::cout << "Interface: " << intf << std::endl;
    std::cout << "Log event: " << ev << std::endl;
    try
    {
        LogServiceProxy logsrvprx(dbuscon);
        if (args->Present("attach"))
        {
            std::cout << "Attatching log" << std::endl;
            logsrvprx.Attach(intf);
            sleep(1); // Wait for the logger to settle this new attachment
        }

        LogSender sig(dbuscon, (LogGroup)lgrp, path, intf);
        auto prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(dbuscon);
        sig.AddTarget(prxqry->GetNameOwner(Constants::GenServiceName("log")));
        sig.SetLogLevel(6);
        sig.Log(ev);
        std::cout << "Log signal sent" << std::endl;

        if (args->Present("attach"))
        {
            sleep(1); // Wait for the logger to process the Log signal
            std::cout << "Detaching log" << std::endl;
            logsrvprx.Detach(intf);
        }
    }
    catch (std::exception &err)
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
    props.reset(new SingleCommand("props", "Gets and sets properties", cmd_props));
    props->AddOption("use-session-bus", 'S', "Use session bus instead of system bus");
    props->AddOption("log-level", 'l', "LEVEL", true, "Sets the log verbosity");
    props->AddOption("timestamp", 't', "BOOLEAN", true, "Sets the timestamp flag for log events. Valid values: true, false");
    props->AddOption("dbus-details", 'D', "BOOLEAN", true, "Sets the D-Bus details logging flag for log events");
    cmds.RegisterCommand(props);

    SingleCommand::Ptr send;
    send.reset(new SingleCommand("send", "Sends log events", cmd_send));
    send->AddOption("use-session-bus", 'S', "Use session bus instead of system bus");
    send->AddOption("attach", 'a', "Do an Attach() method call before sending log event");
    send->AddOption("object-path", 'o', "PATH", true, "D-Bus path to use as the signal origin (default: /net/openvpn/v3/logtest)");
    send->AddOption("interface", 'i', "STRING", true, "Interface string to use when sending log events (default: net.openvpn.v3.logtest");
    send->AddOption("group", 'g', "INTEGER", true, "LogGroup value to use for the log event");
    send->AddOption("category", 'c', "INTEGER", true, "LogCategory value to use for the log event");
    cmds.RegisterCommand(send);

    try
    {
        return cmds.ProcessCommandLine(argc, argv);
    }
    catch (CommandException &e)
    {
        if (e.gotErrorMessage())
        {
            std::cerr << e.getCommand() << ": ** ERROR ** " << e.what() << std::endl;
        }
        return 9;
    }
}
