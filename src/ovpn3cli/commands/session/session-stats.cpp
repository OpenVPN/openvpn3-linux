//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session-stats.cpp
 *
 * @brief  Provide the current session statistics for a single
 *         running VPN session
 */

#include <string>
#include <gdbuspp/connection.hpp>

#include "common/cmdargparser.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../../arghelpers.hpp"
#include "helpers.hpp"

using namespace ovpn3cli::session;


/**
 *  Similar to statistics_plain(), but returns a JSON string blob with the
 *  statistics data
 *
 * @param stats  The ConnectionStats object returned by fetch_stats()
 * @return Returns std::string with the statistics pre-formatted as JSON
 *  */
static std::string statistics_json(ConnectionStats &stats)
{
    Json::Value outdata;

    for (auto &sd : stats)
    {
        outdata[sd.key] = (Json::Value::Int64)sd.value;
    }
    std::stringstream res;
    res << outdata;
    res << std::endl;
    return res.str();
}


/**
 *  openvpn3 session-stats command
 *
 *  Dumps statistics for a specific VPN session in either text/plain or JSON
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 */
static int cmd_session_stats(ParsedArgs::Ptr args)
{
    if (!args->Present("path") && !args->Present("config")
        && !(args->Present("interface")))
    {
        throw CommandException("session-stats",
                               "Missing required session path, config "
                               "or interface name");
    }

    try
    {
        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);

        DBus::Object::Path sesspath{};
        if (args->Present("config"))
        {
            auto paths = sessmgr->LookupConfigName(args->GetValue("config", 0));
            if (0 == paths.size())
            {
                throw CommandException("session-stats",
                                       "No sessions started with the "
                                       "configuration profile name was found");
            }
            else if (1 < paths.size())
            {
                throw CommandException("session-stats",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            sesspath = paths.at(0);
        }
        else if (args->Present("interface"))
        {
            try
            {
                sesspath = sessmgr->LookupInterface(args->GetValue("interface", 0));
            }
            catch (const DBus::Exception &excp)
            {
                throw CommandException("session-stats", excp.GetRawError());
            }
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        try
        {
            auto session = sessmgr->Retrieve(sesspath);
            ConnectionStats stats = session->GetConnectionStats();

            std::cout << (args->Present("json") ? statistics_json(stats)
                                                : statistics_plain(stats));
        }
        catch (const DBus::Exception &excp)
        {
            throw CommandException("session-stats", excp.GetRawError());
        }

        return 0;
    }
    catch (...)
    {
        throw;
    }
}

/**
 *  Creates the SingleCommand object for the 'session-stats' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_stats()
{
    //
    //  session-stats command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-stats",
                                "Show session statistics",
                                cmd_session_stats));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "SESSION-PATH",
                                   true,
                                   "Path to the configuration in the "
                                   "configuration manager",
                                   arghelper_session_paths);
    path_opt->SetAlias("session-path");
    cmd->AddOption("config",
                   'c',
                   "CONFIG-NAME",
                   true,
                   "Alternative to --path, where configuration profile name "
                   "is used instead",
                   arghelper_config_names_sessions);
    cmd->AddOption("interface",
                   'I',
                   "INTERFACE",
                   true,
                   "Alternative to --path, where tun interface name is used "
                   "instead",
                   arghelper_managed_interfaces);
    cmd->AddOption("json",
                   'j',
                   "Dump the configuration in JSON format");

    return cmd;
}
