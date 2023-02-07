//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
//
//

/**
 * @file   journal.cpp
 *
 * @brief  Admin command to retrieve OpenVPN 3 Linux log entries
 *         from the systemd-journald
 */

#include "config.h"

#ifdef HAVE_SYSTEMD
#include <iostream>
#include "common/cmdargparser.hpp"
#include "log/journal-log-parse.hpp"

using namespace Log::Journald;

int cmd_journal(ParsedArgs::Ptr args)
{
    if (::geteuid() != 0)
    {
        throw CommandException("journal",
                               "This command requires root privileges");
    }

    Parse journal;

    // Prepare the optional filters
    if (args->Present("since"))
    {
        journal.AddFilter(Parse::FilterType::TIMESTAMP, args->GetValue("since", 0));
    }
    if (args->Present("path"))
    {
        journal.AddFilter(Parse::FilterType::OBJECT_PATH, args->GetValue("path", 0));
    }
    if (args->Present("sender"))
    {
        journal.AddFilter(Parse::FilterType::SENDER, args->GetValue("sender", 0));
    }
    if (args->Present("interface"))
    {
        journal.AddFilter(Parse::FilterType::INTERFACE, args->GetValue("interface", 0));
    }
    if (args->Present("logtag"))
    {
        journal.AddFilter(Parse::FilterType::LOGTAG, args->GetValue("logtag", 0));
    }
    if (args->Present("session-token"))
    {
        journal.AddFilter(Parse::FilterType::SESSION_TOKEN, args->GetValue("session-token", 0));
    }

    LogEntries recs = journal.Retrieve();
    if (args->Present("json"))
    {
        std::cout << recs.GetJSON() << std::endl;
    }
    else
    {
        std::cout << recs;
    }
    return 0;
}

/**
 *  Creates the SingleCommand object for the 'journal' admin command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_journal()
{
    SingleCommand::Ptr jrnlcmd;
    jrnlcmd.reset(new SingleCommand("journal",
                                    "Retrieve OpenVPN 3 Linux journal log entries",
                                    cmd_journal));
    jrnlcmd->AddOption("json",
                       "Format the result as JSON");
    jrnlcmd->AddOption("since", "DATE/TIME", true, "Retrieve log entries after the provided DATE/TIME value");
    jrnlcmd->AddOption("path", "DBUS_OBJECT_PATH", true, "Filter on D-Bus object path");
    jrnlcmd->AddOption("sender", "DBUS_SENDER", true, "Filter on D-Bus unique bus name (:x.xx)");
    jrnlcmd->AddOption("interface", "DBUS_INTERFACE", true, "Filter on D-Bus object interface name");
    jrnlcmd->AddOption("logtag", "LOGTAG", true, "Filter on the LogTag value of the service");
    jrnlcmd->AddOption("session-token", "TOKEN", true, "Filter on the VPN client process session token value");

    return jrnlcmd;
}

#endif // HAVE_SYSTEMD