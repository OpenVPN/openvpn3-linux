//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   session.hpp
 *
 * @brief  Commands to start and manage VPN sessions
 */

#include <csignal>
#include <json/json.h>

#include "dbus/core.hpp"
#include "common/cmdargparser.hpp"
#include "common/lookup.hpp"
#include "common/open-uri.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "../arghelpers.hpp"


class SessionException : public CommandArgBaseException
{
  public:
    SessionException(const std::string msg)
        : CommandArgBaseException(msg)
    {
    }
};


/**
 *  Fetches all the gathered statistics for a specific session
 *
 * @param session_path  std::string containing the D-Bus session path
 * @return Returns the statistics as the ConnectionStats type.
 */
static ConnectionStats fetch_stats(std::string session_path)
{
    try
    {
        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, session_path);
        if (!session.CheckObjectExists())
        {
            throw CommandException("session-stats",
                                   "Session not found");
        }
        return session.GetConnectionStats();
    }
    catch (DBusException &err)
    {
        std::stringstream errmsg;
        errmsg << "Failed to fetch statistics: " << err.GetRawError();
        throw CommandException("session-stats", errmsg.str());
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


/**
 *  Similiar to statistics_plain(), but returns a JSON string blob with the
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


/**
 *  Defines which start modes used by @start_session()
 */
enum class SessionStartMode : std::uint8_t
{
    NONE,   /**< Undefined */
    START,  /**< Call the Connect() method */
    RESUME, /**< Call the Resume() method */
    RESTART /**< Call the Restart() method */
};



/**
 *  Start, resume or restart a VPN session
 *
 *  This code is shared among all the start methods to ensure the
 *  user gets a chance to provide user input if the remote server
 *  wants that for re-authentication.
 *
 * @param session       OpenVPN3SessionProxy to the session object to
 *                      operate on
 * @param initial_mode  SessionStartMode defining how this tunnel is
 *                      to be (re)started
 * @param timeout       Connection timeout. If exceeding this, the connection
 *                      attempt is aborted.
 * @param background    Connection should be started in the background after
 *                      basic credentials has been provided.  Only considered
 *                      for initial_mode == SessionStartMode::START.
 *
 * @throws SessionException if any issues related to the session itself.
 */
static void start_session(OpenVPN3SessionProxy::Ptr session,
                          SessionStartMode initial_mode,
                          int timeout,
                          bool background = false)
{
    // Prepare the SIGINT signal handling
    struct sigaction sact;
    sact.sa_handler = sigint_handler;
    sact.sa_flags = 0;
    sigemptyset(&sact.sa_mask);
    sigaction(SIGINT, &sact, NULL);

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
            StatusEvent s;
            while ((-1 == timeout) || ((op_start + timeout) >= time(0)))
            {
                usleep(300000); // sleep 0.3 seconds - avg setup time
                try
                {
                    s = session->GetLastStatus();
                }
                catch (DBusException &excp)
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
                    throw ReadyException(s.message);
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
        catch (const ReadyException &)
        {
            // If the ReadyException is thrown, it means the backend
            // needs more from the front-end side
            for (auto &type_group : session->QueueCheckTypeGroup())
            {
                ClientAttentionType type;
                ClientAttentionGroup group;
                std::tie(type, group) = type_group;

                if (ClientAttentionType::CREDENTIALS == type)
                {
                    sigaction(SIGINT, &sact, NULL);
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
                                    loops = 10;
                                }
                                done = true;
                            }
                            catch (const DBusException &excp)
                            {
                                std::string err(excp.what());
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
                    return;
                }
            }
        }
        catch (DBusException &err)
        {
            std::stringstream errm;
            errm << "Failed to start new session: "
                 << err.GetRawError();
            throw SessionException(errm.str());
        }
    }
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
        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        std::string sesspath = "";
        if (args->Present("config"))
        {
            std::vector<std::string> paths = sessmgr.LookupConfigName(args->GetValue("config", 0));
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
                sesspath = sessmgr.LookupInterface(args->GetValue("interface", 0));
            }
            catch (DBusException &excp)
            {
                throw CommandException("session-stats", excp.GetRawError());
            }
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        ConnectionStats stats = fetch_stats(sesspath);

        std::cout << (args->Present("json") ? statistics_json(stats)
                                            : statistics_plain(stats));
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



//////////////////////////////////////////////////////////////////////////


// Implemented in config-import.cpp
std::string import_config(const std::string &filename,
                          const std::string &cfgname,
                          const bool single_use,
                          const bool persistent,
                          const std::vector<std::string> &tags);

/**
 *  openvpn3 session-start command
 *
 *  This command is used to initate and start a new VPN session
 *
 * @param args
 * @return
 */
static int cmd_session_start(ParsedArgs::Ptr args)
{
    if (!args->Present("config-path") && !args->Present("config"))
    {
        throw CommandException("session-start",
                               "Either --config or --config-path must be provided");
    }
    if (args->Present("config-path") && args->Present("config"))
    {
        throw CommandException("session-start",
                               "--config and --config-path cannot be used together");
    }
    if (args->Present("persist-tun") && !args->Present("config"))
    {
        throw CommandException("session-start",
                               "--persist-tun can only be used with --config");
    }

    int timeout = -1;
    if (args->Present("timeout"))
    {
        timeout = std::atoi(args->GetValue("timeout", 0).c_str());
    }

    try
    {
        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        std::string cfgpath;
        if (args->Present("config"))
        {
            // This will first lookup the configuration name in the
            // Configuration Manager before attempting to load a local file.
            // If multiple configurations carry the same configuration name,
            // it will fail as we do not support handling this scenario.  It

            std::string cfgname = args->GetValue("config", 0);
            try
            {
                cfgpath = retrieve_config_path("session-start", cfgname);
                std::cout << "Using pre-loaded configuration profile '"
                          << cfgname << "'" << std::endl;
            }
            catch (const CommandException &excp)
            {
                std::string err(excp.what());
                if (err.find("More than one configuration profile was found") != std::string::npos)
                {
                    throw;
                }
                cfgpath = import_config(cfgname, cfgname, true, false, {});
                std::cout << "Using configuration profile from file: "
                          << cfgname << std::endl;
            }
        }
        else
        {
            cfgpath = args->GetValue("config-path", 0);
        }

        // If --persist-tun is given on the command line, enforce this
        // feature on this connection.  This can only be provided when using
        // --config, not --config-path.
        if (args->Present("persist-tun"))
        {
            OpenVPN3ConfigurationProxy cfgprx(G_BUS_TYPE_SYSTEM, cfgpath);
            const ValidOverride &vo = cfgprx.LookupOverride("persist-tun");
            cfgprx.SetOverride(vo, true);
        }

        // Create a new tunnel session
        OpenVPN3SessionProxy::Ptr session = sessmgr.NewTunnel(cfgpath);

        std::cout << "Session path: " << session->GetPath() << std::endl;

#ifdef ENABLE_OVPNDCO
        if (args->Present("dco"))
        {
            // If a certain DCO state was requested, set the DCO flag before
            // starting the connection
            session->SetDCO(args->GetBoolValue("dco", false));
        }
#endif

        start_session(session, SessionStartMode::START, timeout, args->Present("background"));
        return 0;
    }
    catch (const SessionException &excp)
    {
        std::string err{excp.what()};
        if (err.find("ERR_PROFILE_SERVER_LOCKED_UNSUPPORTED:") != std::string::npos)
        {
            throw CommandException("session-start", "Server locked profiles are unsupported");
        }
        throw CommandException("session-start", excp.what());
    }
    catch (...)
    {
        throw;
    }
}

/**
 *  Creates the SingleCommand object for the 'session-start' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_start()
{
    //
    //  session-start command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-start",
                                "Start a new VPN session",
                                cmd_session_start));
    cmd->AddOption("config",
                   'c',
                   "CONFIG-FILE",
                   true,
                   "Configuration file to start directly",
                   arghelper_config_names);
    cmd->AddOption("config-path",
                   'p',
                   "CONFIG-PATH",
                   true,
                   "Configuration path to an already imported configuration",
                   arghelper_config_paths);
    cmd->AddOption("persist-tun",
                   0,
                   "Enforces persistent tun/seamless tunnel (requires --config)");
    cmd->AddOption("timeout",
                   0,
                   "SECS",
                   true,
                   "Connection attempt timeout (default: infinite)");
    cmd->AddOption("background",
                   0,
                   "Starts the connection in the background after basic credentials are provided");
#ifdef ENABLE_OVPNDCO
    cmd->AddOption("dco",
                   0,
                   "BOOL",
                   true,
                   "Start the connection using Data Channel Offload kernel acceleration",
                   arghelper_boolean);
#endif

    return cmd;
}



//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 session-manage command
 *
 *  This command handles pausing, resuming and disconnecting established
 *  sessions.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 */
static int cmd_session_manage(ParsedArgs::Ptr args)
{
    // clang-format off
    const unsigned int mode_pause      = 1 << 0;
    const unsigned int mode_resume     = 1 << 1;
    const unsigned int mode_restart    = 1 << 2;
    const unsigned int mode_disconnect = 1 << 3;
    const unsigned int mode_cleanup    = 1 << 4;
    const unsigned int mode_log_level  = 1 << 5;
    // clang-format on

    unsigned int mode = 0;
    unsigned int mode_count = 0;
    if (args->Present("pause"))
    {
        mode |= mode_pause;
        mode_count++;
    }
    if (args->Present("resume"))
    {
        mode |= mode_resume;
        mode_count++;
    }
    if (args->Present("restart"))
    {
        mode |= mode_restart;
        mode_count++;
    }
    if (args->Present("disconnect"))
    {
        mode |= mode_disconnect;
        mode_count++;
    }
    if (args->Present("cleanup"))
    {
        mode |= mode_cleanup;
        mode_count++;
    }

    if (args->Present("log-level"))
    {
        mode |= mode_log_level;
        mode_count++;
    }

    if (0 == mode_count)
    {
        throw CommandException("session-manage",
                               "One of --pause, --resume, --restart, "
                               "--disconnect, --cleanup or --log-level "
                               "must be present");
    }
    if (1 < mode_count)
    {
        throw CommandException("session-manage",
                               "--pause, --resume, --restart, --disconnect "
                               "--cleanup or --log-level "
                               "cannot be used together");
    }

    // Only --cleanup does NOT depend on --path or --config
    if (!args->Present("path") && !args->Present("config")
        && !args->Present("interface") && (mode ^ mode_cleanup) > 0)
    {
        throw CommandException("session-manage",
                               "Missing required session path or config name");
    }


    try
    {
        int timeout = -1;
        if (args->Present("timeout"))
        {
            timeout = std::atoi(args->GetValue("timeout", 0).c_str());
        }

        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        if (mode_cleanup == mode)
        {
            // Loop through all open sessions and check if they have a valid
            // status available.  A valid status means it is not empty nor
            // unset.  If the status can't be retrieved, it is also
            // invalid.
            std::vector<OpenVPN3SessionProxy::Ptr> sessions = sessmgr.FetchAvailableSessions();
            std::cout << "Cleaning up stale sessions - Found "
                      << std::to_string(sessions.size()) << " open sessions "
                      << "to check" << std::endl;

            unsigned int c = 0;
            for (const auto &s : sessions)
            {
                try
                {
                    std::string cfgname = s->GetStringProperty("config_name");

                    std::cout << "Checking:  " << cfgname << " - " << s->GetPath()
                              << " ... ";

                    StatusEvent st = s->GetLastStatus();
                    if (st.Check(StatusMajor::UNSET, StatusMinor::UNSET)
                        && st.message.empty())
                    {
                        // This is an empty and unset status
                        // These are rare, as it means the
                        // openvpn3-service-client process is running and
                        // responsive, but has not even managed to load a
                        // configuration profile
                        s->Disconnect();
                        std::cout << "Removed" << std::endl;
                        ++c;
                    }
                    else
                    {
                        std::cout << "Valid, keeping it" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    // Errors in this case indicates we cannot retrieve any
                    // information about the session; thus it is most likely
                    // not valid any more, so we kill it - and ignore any
                    // errors while killing it.
                    //
                    // When this happens the openvpn3-service-client process
                    // is most likely already dead.  Calling the Disconnect()
                    // method basically just cleans up the session in the
                    // Session Manager only.
                    try
                    {
                        s->Disconnect();
                    }
                    catch (...)
                    {
                    }
                    std::cout << "Removed" << std::endl;
                    ++c;
                }
            }
            std::cout << std::to_string(c) << " session" << (c != 1 ? "s" : "")
                      << " removed" << std::endl;
            return 0;
        }


        std::string sesspath = "";
        if (args->Present("config"))
        {
            std::vector<std::string> paths = sessmgr.LookupConfigName(args->GetValue("config", 0));
            if (0 == paths.size())
            {
                throw CommandException("session-manage",
                                       "No sessions started with the "
                                       "configuration profile name was found");
            }
            else if (1 < paths.size())
            {
                throw CommandException("session-manage",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            sesspath = paths.at(0);
        }
        else if (args->Present("interface"))
        {
            sesspath = sessmgr.LookupInterface(args->GetValue("interface", 0));
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        OpenVPN3SessionProxy::Ptr session;
        session.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, sesspath));

        if (!session->CheckObjectExists())
        {
            throw CommandException("session-manage",
                                   "Session not found");
        }

        ConnectionStats stats;

        switch (mode)
        {
        case mode_pause:
            session->Pause("Front-end request");
            std::cout << "Initiated session pause: " << sesspath
                      << std::endl;
            return 0;

        case mode_resume:
            std::cout << "Resuming session: " << sesspath
                      << std::endl;
            start_session(session, SessionStartMode::RESUME, timeout, timeout < 0);
            return 0;

        case mode_restart:
            std::cout << "Restarting session: " << sesspath
                      << std::endl;
            start_session(session, SessionStartMode::RESTART, timeout, timeout < 0);
            return 0;

        case mode_disconnect:
            try
            {
                stats = session->GetConnectionStats();
            }
            catch (...)
            {
                std::cout << "Connection statistics is not available"
                          << std::endl;
            }
            session->Disconnect();
            std::cout << "Initiated session shutdown." << std::endl;
            std::cout << statistics_plain(stats);
            break;

        case mode_log_level:
            unsigned int cur = session->GetLogVerbosity();
            std::cout << "Current log level: " << std::to_string(cur) << std::endl;

            if (args->GetValueLen("log-level") > 0)
            {
                unsigned int verb = std::atoi(args->GetLastValue("log-level").c_str());
                if (verb != cur)
                {
                    session->SetLogVerbosity(verb);
                    std::cout << "New log level: " << std::to_string(verb)
                              << std::endl;
                }
            }
            break;
        }
        return 0;
    }
    catch (const DBusException &excp)
    {
        throw CommandException("session-manage", excp.GetRawError());
    }
    catch (const SessionException &excp)
    {
        throw CommandException("session-manage", excp.what());
    }
    catch (...)
    {
        throw;
    }
}

/**
 *  Creates the SingleCommand object for the 'session-manage' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_manage()
{
    //
    //  session-manage command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-manage",
                                "Manage VPN sessions",
                                cmd_session_manage));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "SESSION-PATH",
                                   true,
                                   "Path to the session in the session "
                                   "manager",
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
    cmd->AddOption("log-level",
                   0,
                   "LEVEL",
                   false,
                   "View/Set the log-level for a running VPN session",
                   arghelper_log_levels);
    cmd->AddOption("timeout",
                   0,
                   "SECS",
                   true,
                   "Connection attempt timeout for resume and restart "

                   "(default: infinite)");
    cmd->AddOption("pause",
                   'P',
                   "Pauses the VPN session");
    cmd->AddOption("resume",
                   'R',
                   "Resumes a paused VPN session");
    cmd->AddOption("restart",
                   "Disconnect and reconnect a running VPN session");
    cmd->AddOption("disconnect",
                   'D',
                   "Disconnects a VPN session");
    cmd->AddOption("cleanup",
                   0,
                   "Clean up stale sessions");

    return cmd;
}



//////////////////////////////////////////////////////////////////////////


static std::string arghelper_auth_req()
{
    OpenVPN3SessionMgrProxy smprx(G_BUS_TYPE_SYSTEM);
    OpenVPN3SessionProxy::Ptr session{nullptr};
    std::ostringstream res;

    for (const auto &sess : smprx.FetchAvailableSessions())
    {
        StatusEvent s = sess->GetLastStatus();
        if (s.Check(StatusMajor::CONNECTION, StatusMinor::CFG_REQUIRE_USER)
            || s.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            res << std::to_string(sess->GetUIntProperty("backend_pid")) << " ";
        }
    }

    return res.str();
}


static int cmd_session_auth_complete(const unsigned int authid)
{
    OpenVPN3SessionMgrProxy smprx(G_BUS_TYPE_SYSTEM);
    OpenVPN3SessionProxy::Ptr session{nullptr};
    for (const auto &s : smprx.FetchAvailableSessions())
    {
        unsigned int bepid = s->GetUIntProperty("backend_pid");
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
              << session->GetStringProperty("config_name")
              << std::endl
              << "Session path: "
              << session->GetPath()
              << std::endl
              << std::endl;
    start_session(session, SessionStartMode::START, -1, false);

    return 0;
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
        return cmd_session_auth_complete(std::atoi(args->GetLastValue("auth-req").c_str()));
    }

    //
    // List all running sessions requiring user authentication interaction
    //
    OpenVPN3SessionMgrProxy smprx(G_BUS_TYPE_SYSTEM);

    // Retrieve a list of all sessions requiring user interaction
    std::vector<OpenVPN3SessionProxy::Ptr> session_list = {};
    for (const auto &sess : smprx.FetchAvailableSessions())
    {
        StatusEvent s = sess->GetLastStatus();
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
        std::string created;
        try
        {
            std::time_t sess_created = sess->GetUInt64Property("session_created");
            std::string c = std::asctime(std::localtime(&sess_created));
            created = c.substr(0, c.size() - 1);
        }
        catch (DBusException &)
        {
            created = "(Not available)";
        }

        auto status = sess->GetLastStatus();
        status.PrintMode(StatusEvent::PrintMode::MINOR);

        std::cout << "        Path: " << sess->GetPath() << std::endl;
        std::cout << "Auth Request: "
                  << std::to_string(sess->GetUIntProperty("backend_pid")) << std::endl;
        std::cout << "     Created: " << created << std::endl;
        std::cout << " Config name: " << sess->GetStringProperty("config_name") << std::endl;
        std::cout << " Auth status: ";
        if (status.Check(StatusMajor::SESSION, StatusMinor::SESS_AUTH_URL))
        {
            status.PrintMode(StatusEvent::PrintMode::NONE);
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


//////////////////////////////////////////////////////////////////////////



/**
 *  openvpn3 session-acl command
 *
 *  Command to modify the access control to a specific VPN session.
 *  All operations related to granting, revoking, public-access are handled
 *  by this command.
 *
 *  Also note that you can run multiple operarations in a single command line.
 *  It is fully possible to use --grant, --revoke and --show in a single
 *  command line.  In addition, both --grant and --revoke can be used multiple
 *  times to grant/revoke several users in a single operation.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_session_acl(ParsedArgs::Ptr args)
{
    int ret = 0;
    if (!args->Present("path") && !args->Present("config")
        && !args->Present("interface"))
    {
        throw CommandException("session-acl",
                               "Missing required session path or config name");
    }

    if (!args->Present("show")
        && !args->Present("grant")
        && !args->Present("revoke")
        && !args->Present("public-access")
        && !args->Present("allow-log-access")
        && !args->Present("lock-down")
        && !args->Present("seal"))
    {
        throw CommandException("session-acl", "No operation option provided");
    }

    try
    {
        OpenVPN3SessionMgrProxy sessmgr(G_BUS_TYPE_SYSTEM);

        std::string sesspath = "";
        if (args->Present("config"))
        {
            std::vector<std::string> paths = sessmgr.LookupConfigName(args->GetValue("config", 0));
            if (0 == paths.size())
            {
                throw CommandException("session-acl",
                                       "No sessions started with the "
                                       "configuration profile name was found");
            }
            else if (1 < paths.size())
            {
                throw CommandException("session-acl",
                                       "More than one session with the given "
                                       "configuration profile name was found.");
            }
            sesspath = paths.at(0);
        }
        else if (args->Present("interface"))
        {
            sesspath = sessmgr.LookupInterface(args->GetValue("interface", 0));
        }
        else
        {
            sesspath = args->GetValue("path", 0);
        }

        OpenVPN3SessionProxy session(G_BUS_TYPE_SYSTEM, sesspath);
        if (!session.CheckObjectExists())
        {
            throw CommandException("session-acl",
                                   "Session not found");
        }

        if (args->Present("grant"))
        {
            for (auto const &user : args->GetAllValues("grant"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        session.AccessGrant(uid);
                        std::cout << "Granted access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException &e)
                    {
                        std::cerr << "Failed granting access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException &)
                {
                    std::cerr << "** ERROR ** --grant " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }

        if (args->Present("revoke"))
        {
            for (auto const &user : args->GetAllValues("revoke"))
            {
                try
                {
                    uid_t uid = get_userid(user);
                    try
                    {
                        session.AccessRevoke(uid);
                        std::cout << "Revoked access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException &e)
                    {
                        std::cerr << "Failed revoking access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
                catch (const LookupException &)
                {
                    std::cerr << "** ERROR ** --revoke " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
            }
        }
        if (args->Present("public-access"))
        {
            std::string ld = args->GetValue("public-access", 0);
            if (("false" == ld) || ("true" == ld))
            {
                session.SetPublicAccess("true" == ld);
                if ("true" == ld)
                {
                    std::cout << "Session is now accessible by everyone"
                              << std::endl;
                }
                else
                {
                    std::cout << "Session is now only accessible to "
                              << "specific users"
                              << std::endl;
                }
            }
            else
            {
                std::cerr << "** ERROR ** --public-access argument must be "
                          << "'true' or 'false'"
                          << std::endl;
                ret = 3;
            }
        }

        if (args->Present("allow-log-access"))
        {
            bool ala = args->GetBoolValue("allow-log-access", 0);
            session.SetRestrictLogAccess(!ala);
            if (ala)
            {
                std::cout << "Session log is now accessible to users granted "
                          << " session access" << std::endl;
            }
            else
            {
                std::cout << "Session log is only accessible the session"
                          << " owner" << std::endl;
            }
        }


        if (args->Present("show"))
        {
            std::string owner = lookup_username(session.GetOwner());
            std::cout << "                    Owner: ("
                      << session.GetOwner() << ") "
                      << " " << ('(' != owner[0] ? owner : "(unknown)")
                      << std::endl;

            std::cout << "            Public access: "
                      << (session.GetPublicAccess() ? "yes" : "no")
                      << std::endl;

            std::cout << " Users granted log access: "
                      << (session.GetRestrictLogAccess() ? "no" : "yes")
                      << std::endl;

            if (!session.GetPublicAccess())
            {
                std::vector<uid_t> acl = session.GetAccessList();
                std::cout << "     Users granted access: " << std::to_string(acl.size())
                          << (1 != acl.size() ? " users" : " user")
                          << std::endl;
                for (auto const &uid : acl)
                {
                    std::string user = lookup_username(uid);
                    std::cout << "                           - (" << uid << ") "
                              << " " << ('(' != user[0] ? user : "(unknown)")
                              << std::endl;
                }
            }
        }
        return ret;
    }
    catch (const DBusException &excp)
    {
        throw CommandException("session-acl", excp.GetRawError());
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  Creates the SingleCommand object for the 'session-acl' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_session_acl()
{
    //
    //  session-acl command
    //
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("session-acl",
                                "Manage access control lists for sessions",
                                cmd_session_acl));
    auto path_opt = cmd->AddOption("path",
                                   'o',
                                   "SESSION-PATH",
                                   true,
                                   "Path to the session in the session "
                                   "manager",
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
    cmd->AddOption("show",
                   's',
                   "Show the current access control lists");
    cmd->AddOption("grant",
                   'G',
                   "<UID | username>",
                   true,
                   "Grant this user access to this session");
    cmd->AddOption("revoke",
                   'R',
                   "<UID | username>",
                   true,
                   "Revoke this user access from this session");
    cmd->AddOption("public-access",
                   "<true|false>",
                   true,
                   "Set/unset the public access flag",
                   arghelper_boolean);
    cmd->AddOption("allow-log-access",
                   "<true|false>",
                   true,
                   "Can users granted access also access the session log?",
                   arghelper_boolean);

    return cmd;
}



//////////////////////////////////////////////////////////////////////////
