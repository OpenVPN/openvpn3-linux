//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session-auth.cpp
 *
 * @brief  Command to pick-up VPN sessions running in the background
 *         requring user interaction for authentication
 */

#include <string>
#include <gdbuspp/connection.hpp>

#include "helpers.hpp"
#include "common/cmdargparser.hpp"
#include "events/status.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../../arghelpers.hpp"

using namespace ovpn3cli::session;


/**
 *  Provides a string of the PID values for all VPN sessions running
 *  requiring the user to authenticate.
 *
 *  This is used by the shell-completion feature in the cmdargparser.cpp
 *
 * @return std::string
 */
static std::string arghelper_auth_req()
{
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto smprx = SessionManager::Proxy::Manager::Create(dbuscon);
    std::ostringstream res;

    for (const auto &sess : smprx->FetchAvailableSessions())
    {
        Events::Status s = sess->GetLastStatus();
        if (s.Check(StatusMajor::CONNECTION, StatusMinor::CFG_REQUIRE_USER)
            || s.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            res << std::to_string(sess->GetBackendPid()) << " ";
        }
    }

    return res.str();
}


/**
 *   Initiate the authentication process for a given session, identified
 *   by the PID of the backend process - aka "Authentication Request ID"
 *
 * @param authid   PID of the backend porcess
 * @return int     The suggested exit code for the program; 0 is success
 */
static int cmd_session_auth_complete(const pid_t authid)
{
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto smprx = SessionManager::Proxy::Manager::Create(dbuscon);
    SessionManager::Proxy::Session::Ptr session = nullptr;
    for (const auto &s : smprx->FetchAvailableSessions())
    {
        pid_t bepid = s->GetBackendPid();
        if (authid == bepid)
        {
            session = s;
            break;
        }
    }
    if (nullptr == session)
    {
        throw CommandException("session-auth", "No session found");
    }

    std::cout << "Continuing authentication for session "
              << session->GetConfigName()
              << std::endl
              << "Session path: "
              << session->GetPath()
              << std::endl
              << std::endl;
    try
    {
        Events::Status status = session->GetLastStatus();
        if (StatusMajor::SESSION == status.major
            && StatusMinor::SESS_AUTH_URL == status.minor)
        {
            if (!start_url_auth(status.message))
            {
                return 2;
            }
        }
        else if (StatusMajor::CONNECTION == status.major
                 && StatusMinor::CFG_REQUIRE_USER == status.minor)
        {
            start_session(session, SessionStartMode::START, -1, false);
        }
        return 0;
    }
    catch (const DBus::Exception &excp)
    {
        std::cerr << "Error: " << excp.GetRawError() << std::endl;
        return 2;
    }
}


/**
 *  openvpn3 session-auth command
 *
 *  This command is used to list on-going VPN sessions requiring user
 *  interaction for further authentication
 *
 */
static int cmd_session_auth(ParsedArgs::Ptr args)
{

    if (args->Present("auth-req"))
    {
        auto reqid = static_cast<pid_t>(std::atoi(args->GetLastValue("auth-req").c_str()));
        return cmd_session_auth_complete(reqid);
    }

    //
    // List all running sessions requiring user authentication interaction
    //
    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto smprx = SessionManager::Proxy::Manager::Create(dbuscon);

    // Retrieve a list of all sessions requiring user interaction
    SessionManager::Proxy::Session::List session_list = {};
    for (const auto &sess : smprx->FetchAvailableSessions())
    {
        Events::Status s = sess->GetLastStatus();
        if (s.Check(StatusMajor::CONNECTION, StatusMinor::CFG_REQUIRE_USER)
            || s.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            session_list.push_back(sess);
        }
    }

    if (session_list.size() < 1)
    {
        std::cout << "No sessions requires authentication interaction" << std::endl;
        return 0;
    }

    // List all sessions found
    std::cout << "-----------------------------------------------------------------------------" << std::endl;
    bool first = true;
    for (const auto &sess : session_list)
    {
        if (!first)
        {
            std::cout << std::endl;
        }
        first = false;

        // Retrieve the session start timestamp
        std::string created{};
        try
        {
            created = sess->GetSessionCreated();
        }
        catch (const DBus::Exception &)
        {
            created = "(Not available)";
        }

        std::cout << "        Path: " << sess->GetPath() << std::endl;
        std::cout << "Auth Request: "
                  << std::to_string(sess->GetBackendPid()) << std::endl;
        std::cout << "     Created: " << created << std::endl;
        std::cout << " Config name: " << sess->GetConfigName() << std::endl;
        std::cout << " Auth status: ";

        auto status = sess->GetLastStatus();
        status.SetPrintMode(Events::Status::PrintMode::MINOR);
        if (status.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            status.SetPrintMode(Events::Status::PrintMode::NONE);
            std::cout << "On-going web authentication" << std::endl
                      << "    Auth URL: ";
        }
        std::cout << status << std::endl;
    }
    std::cout << "-----------------------------------------------------------------------------" << std::endl;
    return 0;
}


SingleCommand::Ptr prepare_command_session_auth()
{
    //
    //  session-auth command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-auth",
                                "Interact with on-going session authentication requests",
                                cmd_session_auth));
    cmd->AddOption("auth-req",
                   0,
                   "ID",
                   true,
                   "Continue the authentication process for the given auth request ID",
                   arghelper_auth_req);
    return cmd;
}
