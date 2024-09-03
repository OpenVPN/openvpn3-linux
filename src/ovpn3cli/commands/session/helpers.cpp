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

SessionException::SessionException(const std::string &msg,
                                   const std::string &details_)
    : CommandArgBaseException(msg), details(details_)
{
}


std::string SessionException::GetDetails() const
{
    return details;
};



enum class ExitReason
{
    NONE,
    CTRL_C,
    ERROR,
    ABORTED,
    DONE
};

/**
 *  namespace global flag to indicate why the program is exiting
 */
ExitReason exit_reason = ExitReason::NONE;

/**
 *  Signal handler called on SIGINT events.  This just sets
 *  an internal flag which needs to be checked later on.
 *
 * @param sig  signal received; this is ignored.
 */
static void sigint_handler(int sig)
{
    exit_reason = ExitReason::CTRL_C;
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
    try
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
                    if (ExitReason::NONE != exit_reason)
                    {
                        break;
                    }
                    while (!done && ExitReason::NONE == exit_reason)
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
                            if (exit_reason == ExitReason::NONE)
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
                                if (ExitReason::CTRL_C != exit_reason)
                                {
                                    std::cerr << "** ERROR **   "
                                              << "Empty input not allowed" << std::endl;
                                }
                                done = true;
                            }
                            else
                            {
                                done = true;
                                exit_reason = ExitReason::ERROR;
                            }
                        }
                    }
                }
            }
        }
        if (ExitReason::ABORTED == exit_reason
            || ExitReason::CTRL_C == exit_reason
            || ExitReason::ERROR == exit_reason)
        {
            std::cerr << "** Aborted **" << std::endl;
            try
            {
                session->Disconnect();
            }
            catch (...)
            {
                // We're shutting down; no need for errors now
            }
            return false;
        }
        return true;
    }
    catch (const DBus::Exception &excp)
    {
        std::string err(excp.what());
        exit_reason = (err.find("No such interface") != std::string::npos
                           ? ExitReason::ABORTED
                           : ExitReason::ERROR);
        return false;
    }
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
                        exit_reason = ExitReason::DONE;
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
                    static const char *err = "Failed to start the connection";
                    if (s.message.find("PEM_PASSWORD_FAIL") != std::string::npos)
                    {
                        throw SessionException(err, "Incorrect Private Key passphrase");
                    }
                    throw SessionException(err, s.message);
                }
                // Check if an SIGINT / CTRL-C event has occurred.
                // If it has, disconnect the connection attempt and abort.
                if (ExitReason::CTRL_C == exit_reason
                    || ExitReason::ERROR == exit_reason)
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
        catch (const SessionManager::Proxy::ReadyException &e)
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
            else if (!query_user_input(session, sact))
            {
                if (ExitReason::ABORTED == exit_reason)
                {
                    throw SessionException("Session aborted through another process");
                }
            };
            loops = 10;
        }
        catch (const DBus::Exception &err)
        {
            std::stringstream errm;
            if (ExitReason::ERROR == exit_reason)
            {
                std::string e = err.GetRawError();
                errm << "Failed to start new session: "
                     << e.substr(e.find("')] ") + 4);
                throw SessionException(errm.str());
            }
            // If a SIGINT was received, there will
            // be errors we should ignore; it's noise for
            // end-users
            return;
        }
    }
}


/**
 *  Converts ConnectionStats into a plain-text string
 *
 * @param stats  The ConnectionStats object returned by fetch_stats()
 * @return Returns std::string with the statistics pre-formatted as text/plain
 */
std::string statistics_plain(ConnectionStats &stats)
{
    if (stats.size() < 1)
    {
        return "";
    }

    std::stringstream out;
    out << std::endl
        << "Connection statistics:" << std::endl;
    for (auto &sd : stats)
    {
        out << "     "
            << sd.key
            << std::setw(20 - sd.key.size()) << std::setfill('.') << "."
            << std::setw(12) << std::setfill('.')
            << sd.value
            << std::endl;
    }
    out << std::endl;
    return out.str();
}

} // namespace ovpn3cli::session
