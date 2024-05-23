//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log.hpp
 *
 * @brief  Commands related to receive log entries from various sessions
 */

#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>

#include "common/cmdargparser.hpp"
#include "../../arghelpers.hpp"

#include "log-attach.hpp"

using namespace ovpn3cli::log;

/**
 *  openvpn3 log
 *
 *  Simple log command which can retrieve Log events happening on a specific
 *  session path or coming from the configuration manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log(ParsedArgs::Ptr args)
{

    if (args->Present({"session-path", "config", "interface"}, true).empty())
    {
        throw CommandException("log",
                               "Either --session-path, --config, --interface "
                               "or must be provided");
    }
    args->CheckExclusiveOptions({{"session-path", "config", "interface"}});

    // Prepare the main loop which will listen for Log events and process them
    auto mainloop = DBus::MainLoop::Create();
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto logattach = LogAttach::Create(mainloop, dbuscon);

    if (args->Present("log-level"))
    {
        logattach->SetLogLevel(std::stoi(args->GetValue("log-level", 0)));
    }

    if (args->Present("config"))
    {
        logattach->AttachByConfig(args->GetValue("config", 0));
    }
    else if (args->Present("interface"))
    {
        logattach->AttachByInterface(args->GetValue("interface", 0));
    }
    else if (args->Present("session-path"))
    {
        logattach->AttachByPath(args->GetValue("session-path", 0));
    }

    // Start the main loop.  This will exit on SIGINT or SIGTERM signals only
    mainloop->Run();

    return 0;
}


std::string arghelper_log_config_names()
{
    return arghelper_config_names() + arghelper_config_names_sessions();
}


SingleCommand::Ptr prepare_command_log()
{
    //
    //  log command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("log",
                                "Receive log events as they occur",
                                cmd_log));
    cmd->AddOption("session-path",
                   "SESSION-PATH",
                   true,
                   "Receive log events for a specific session",
                   arghelper_session_paths);
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --session-path, where configuration "
                   "profile name is used instead",
                   arghelper_log_config_names);
    cmd->AddOption("interface",
                   'I',
                   "INTERFACE",
                   true,
                   "Alternative to --session-path, where tun interface name "
                   "is used instead",
                   arghelper_managed_interfaces);
    cmd->AddOption("log-level",
                   "LOG-LEVEL",
                   true,
                   "Set the log verbosity level of messages to be shown (default: 4)",
                   arghelper_log_levels);

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
