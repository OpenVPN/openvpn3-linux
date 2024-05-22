//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   helpers.cpp
 *
 * @brief  Misc helper functions and related types for
 *         the openvpn3 session commands
 */

#include <csignal>

#include "common/open-uri.hpp"
#include "helpers.hpp"


namespace ovpn3cli::session {

SessionException::SessionException(const std::string &msg)
    : CommandArgBaseException(msg)
{
}



bool sigint_received = false; /**< Global flag indicating SIGINT event */

/**
 *  Signal handler called on SIGINT events.  This just sets
 *  an internal flag which needs to be checked later on.
 *
 * @param sig  signal received; this is ignored.
 */
static void sigint_handler(int sig)
{
    sigint_received = true;
    std::cout << "!!" << std::endl;
}


bool start_url_auth(const std::string &url)
{
    OpenURIresult r = open_uri(url);
    switch (r->status)
    {
    case OpenURIstatus::INVALID:
        std::cout << "** ERROR **  The server requested an invalid URL: "
                  << url << std::endl;
        return false;

    case OpenURIstatus::FAIL:
        std::cout << "Could not open the URL automatically." << std::endl
                  << "Open this URL to complete the connection: " << std::endl
                  << "     " << url << std::endl
                  << std::endl
                  << "Further manage this session using 'openvpn3 session-auth'"
                  << std::endl;
        return true;

    case OpenURIstatus::SUCCESS:
        std::cout << "Session running, awaiting external authentication." << std::endl
                  << "Further manage this session using "
                  << "'openvpn3 session-manage' and 'openvpn3 session-auth'"
                  << std::endl;
        return true;

    default:
        std::cout << "** ERROR **  Unknown error occurred." << std::endl
                  << r->message << std::endl;
        return false;
    }
}


bool query_user_input(SessionManager::Proxy::Session::Ptr session,
                      struct sigaction *sigact)
{
    for (auto &type_group : session->QueueCheckTypeGroup())
    {
        ClientAttentionType type;
        ClientAttentionGroup group;
        std::tie(type, group) = type_group;

        if (ClientAttentionType::CREDENTIALS == type)
        {
            if (sigact)
            {
                sigaction(SIGINT, sigact, NULL);
            }
            std::vector<struct RequiresSlot> reqslots;
            session->QueueFetchAll(reqslots, type, group);
            for (auto &r : reqslots)
            {
                bool done = false;
                if (sigint_received)
                {
                    break;
                }
                while (!done && !sigint_received)
                {
                    try
                    {
                        std::cout << r.user_description << ": ";
                        if (r.hidden_input)
                        {
                            set_console_echo(false);
                        }
                        std::getline(std::cin, r.value);
                        if (r.hidden_input)
                        {
                            std::cout << std::endl;
                            set_console_echo(true);
                        }
                        if (!sigint_received)
                        {
                            session->ProvideResponse(r);
                        }
                        done = true;
                    }
                    catch (const DBus::Exception &excp)
                    {
                        std::string err(excp.GetRawError());
                        if (err.find("No value provided for") != std::string::npos)
                        {
                            std::cerr << "** ERROR **   Empty input not allowed" << std::endl;
                        }
                        else
                        {
                            done = true;
                            sigint_received = true;
                        }
                    }
                }
            }
        }
        if (sigint_received)
        {
            std::cerr << "** Aborted **" << std::endl;
            session->Disconnect();
            return false;
        }
    }
    return true;
}


void start_session(SessionManager::Proxy::Session::Ptr session,
                   SessionStartMode initial_mode,
                   int timeout,
                   bool background)
{
    // Prepare the SIGINT signal handling
    auto sact = new struct sigaction;
    sact->sa_handler = sigint_handler;
    sact->sa_flags = 0;
    sigemptyset(&sact->sa_mask);
    sigaction(SIGINT, sact, NULL);

    // Start or restart the session
    SessionStartMode mode = initial_mode;
    unsigned int loops = 10;
    while (loops > 0)
    {
        loops--;
        try
        {
            session->Ready(); // If not, an exception will be thrown
            switch (mode)
            {
            case SessionStartMode::START:
                session->Connect();
                break;

            case SessionStartMode::RESUME:
                session->Resume();
                mode = SessionStartMode::START;
                break;

            case SessionStartMode::RESTART:
                session->Restart();
                mode = SessionStartMode::START;
                break;

            default:
                // This should never be triggered
                throw SessionException("Unknown SessionStartMode");
            }

            if (background)
            {
                std::cout << "Session is running in the background" << std::endl;
                return;
            }

            // Attempt to connect until the given timeout has been reached.
            // If timeout has been disabled (-1), loop forever.
            time_t op_start = time(0);
            Events::Status s;
            while ((-1 == timeout) || ((op_start + timeout) >= time(0)))
            {
                usleep(300000); // sleep 0.3 seconds - avg setup time
                try
                {
                    s = session->GetLastStatus();
                }
                catch (const DBus::Exception &excp)
                {
                    std::string err(excp.what());
                    if (err.find("Failed retrieving property value for 'status'") != std::string::npos)
                    {
                        throw SessionException("Failed to start session");
                    }
                    throw;
                }
                if (s.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
                {
                    std::cout << "Web based authentication required." << std::endl;
                    if (start_url_auth(s.message))
                    {
                        return;
                    }
                    else
                    {
                        sigint_received = true; // Simulate a CTRL-C to exit
                        std::cout << "Disconnecting" << std::endl;
                    }
                }
                else if (s.minor == StatusMinor::CONN_CONNECTED)
                {
                    std::cout << "Connected" << std::endl;
                    return;
                }
                else if (s.minor == StatusMinor::CONN_DISCONNECTED)
                {
                    break;
                }
                else if (s.minor == StatusMinor::CONN_AUTH_FAILED)
                {
                    try
                    {
                        session->Disconnect();
                    }
                    catch (...)
                    {
                        // Ignore errors
                    }
                    throw SessionException("User authentication failed");
                }
                else if (s.minor == StatusMinor::CFG_REQUIRE_USER)
                {
                    throw SessionManager::Proxy::ReadyException(s.message);
                }
                else if (s.minor == StatusMinor::CONN_FAILED)
                {
                    try
                    {
                        {
                            session->Disconnect();
                        }
                    }
                    catch (...)
                    {
                        // ignore errors
                    }
                    std::string err = "Failed to start the connection";
                    if (s.message.find("PEM_PASSWORD_FAIL") != std::string::npos)
                    {
                        err += ", incorrect Private Key passphrase";
                    }
                    throw SessionException(err);
                }
                // Check if an SIGINT / CTRL-C event has occurred.
                // If it has, disconnect the connection attempt and abort.
                if (sigint_received)
                {
                    try
                    {
                        session->Disconnect();
                    }
                    catch (...)
                    {
                        // Ignore any errors in this case
                    }
                    std::cout << std::endl;
                    throw SessionException("Session stopped");
                }

                sleep(1); // If not yet connected, wait for 1 second
            }
            time_t now = time(0);
            if ((op_start + timeout) <= now)
            {
                std::stringstream err;
                err << "Failed to connect"
                    << (timeout > 0 && (op_start + timeout) <= now ? " (timeout)" : "")
                    << ": " << s << std::endl;
                session->Disconnect();
                throw SessionException(err.str());
            }
        }
        catch (const SessionManager::Proxy::ReadyException &)
        {
            // If the ReadyException is thrown, it means the backend
            // needs more from the front-end side

            Events::Status s = session->GetLastStatus();
            if (StatusMajor::SESSION == s.major
                && StatusMinor::SESS_AUTH_URL == s.minor)
            {
                // We don't need to consider any errors here;
                // this function should exit regardless and the
                // error message is handled in start_url_auth()
                (void)start_url_auth(s.message);
                std::cout << "Disconnecting" << std::endl;
                return;
            }
            else if (query_user_input(session, sact))
            {
                loops = 10;
            };
        }
        catch (const DBus::Exception &err)
        {
            std::stringstream errm;
            errm << "Failed to start new session: "
                 << err.GetRawError();
            throw SessionException(errm.str());
        }
    }
}


} // namespace ovpn3cli::session
