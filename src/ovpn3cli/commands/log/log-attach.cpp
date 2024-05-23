//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log/log-attach.hpp
 *
 * @brief  Helper class to prepare setting up the EventLogger object
 */

#include "common/cmdargparser-exceptions.hpp"
#include "log-attach.hpp"


namespace ovpn3cli::log {

LogAttach::Ptr LogAttach::Create(DBus::MainLoop::Ptr mainloop,
                      DBus::Connection::Ptr dbuscon)
{
    return Ptr(new LogAttach(mainloop, dbuscon));
}


LogAttach::LogAttach(DBus::MainLoop::Ptr main_loop,
                     DBus::Connection::Ptr dbusc)
    : mainloop(main_loop), dbuscon(dbusc)
{
    signal_subscr = DBus::Signals::SubscriptionManager::Create(dbuscon);
    auto sessionmgr_tgt = DBus::Signals::Target::Create(
        Constants::GenServiceName("sessions"),
        Constants::GenPath("sessions"),
        Constants::GenInterface("sessions"));
    signal_subscr->Subscribe(sessionmgr_tgt,
                             "SessionManagerEvent",
                             [&](DBus::Signals::Event::Ptr &event)
                             {
                                 SessionManager::Event smgrev(event->params);
                                 sessionmgr_event(smgrev);
                             });

    manager = SessionManager::Proxy::Manager::Create(dbuscon);
}


LogAttach::~LogAttach() noexcept
{
    if (session_proxy)
    {
        try
        {
            session_proxy->LogForward(false);
            usleep(500000);
        }
        catch (const DBus::Exception &)
        {
            // This may fail if the sessoin is already closed
            // No need to make fuzz about that
        }
    }
}


void LogAttach::AttachByPath(const DBus::Object::Path &path)
{
    session_path = path;
    setup_session_logger(session_path);
}


void LogAttach::AttachByConfig(const std::string &config)
{
    config_name = config;
    lookup_config_name(config_name);
    setup_session_logger(session_path);
}


void LogAttach::AttachByInterface(const std::string &interf)
{
    tun_interf = interf;
    lookup_interface(tun_interf);
    setup_session_logger(session_path);
}


void LogAttach::SetLogLevel(const unsigned int loglvl)
{
    log_level = loglvl;
}



void LogAttach::sessionmgr_event(const SessionManager::Event &event)
{
    switch (event.type)
    {
    case SessionManager::EventType::SESS_CREATED:
        if (session_proxy)
        {
            // If we already have a running session proxy,
            // ignore new sessions
            return;
        }
        if (!config_name.empty())
        {
            lookup_config_name(config_name);
        }
        else if (!tun_interf.empty())
        {
            lookup_interface(tun_interf);
        }

        setup_session_logger(session_path);
        break;

    case SessionManager::EventType::SESS_DESTROYED:
        if (0 != event.path.compare(session_path))
        {
            // This event is not related to us, ignore it
            return;
        }
        std::cout << "Session closed" << std::endl;
        mainloop->Stop();
        break;

    case SessionManager::EventType::UNSET:
    default:
        // Ignore unknown types silently
        break;
    }
}


void LogAttach::lookup_config_name(const std::string &cfgname)
{
    // We need to try a few times, as the SESS_CREATED event comes
    // quite early and the session object itself might not be registered
    // quite yet.  This needs to be a tight loop to catch the earliest
    // log messages, as certificate only based authentication may do
    // the full connection establishing in a little bit less than 200ms.
    for (unsigned int i = 250; i > 0; i--)
    {
        DBus::Object::Path::List paths = manager->LookupConfigName(cfgname);
        if (1 < paths.size())
        {
            throw CommandException("log",
                                   "More than one session with the given "
                                   "configuration profile name was found.");
        }
        else if (1 == paths.size())
        {
            // If only a single path is found, that's the one we're
            // looking for.
            session_path = paths.at(0);
            return;
        }
        else
        {
            // If no paths has been found, we wait until next call
            if (!wait_notification)
            {
                std::cout << "Waiting for session to start ..."
                          << std::flush;
                wait_notification = true;
            }
        }
        usleep(50000); // 50ms
    }
}


void LogAttach::lookup_interface(const std::string &interf)
{
    // This method does not have any retry logic as @lookup_config_name()
    // the tun interface name will appear quite late in the connection
    // logic, making it less useful for non-established VPN sessions.
    // The tun device name is first known after a successful connection.
    try
    {
        session_path = manager->LookupInterface(interf);
    }
    catch (const SessionManager::Proxy::Exception &excp)
    {
        throw CommandException("log", excp.GetRawError());
    }
    catch (const DBus::Exception &excp)
    {
        throw CommandException("log", excp.GetRawError());
    }
    catch (const std::exception &excp)
    {
        throw CommandException("log", std::string(excp.what()));
    }
}


/**
 *  Create a new SessionLogger object for processing log and status event
 *  changes for a running session.
 *
 *  The input path is used to ensure a SESS_CREATED SessionManager::Event
 *  is tied to the VPN session we expect.
 *
 * @param path  DBus::Object::Path to the VPN session to attach to.
 */
void LogAttach::setup_session_logger(const DBus::Object::Path &path)
{
    if (path.empty()
        || (0 != session_path.compare(path)))
    {
        return;
    }

    if (wait_notification)
    {
        std::cout << " Done" << std::endl;
    }

    // Sanity check before setting up the session logger; does the
    // session exist?
    session_proxy = manager->Retrieve(path);
    if (!session_proxy->CheckSessionExists())
    {
        throw CommandException("log",
                               "Session not found");
    }

    std::cout << "Attaching to session " << path << std::endl;
    // Change the log-level if requested.
    if (log_level > 0)
    {
        try
        {
            // FIXME: This is not enough, consider the log proxy attachment
            session_proxy->SetLogVerbosity(log_level);
        }
        catch (std::exception &e)
        {
            // Log level might be incorrect, but we don't exit because
            // of that - as it might be we've been waiting for a session
            // to start.
            std::cerr << "** WARNING ** Log level not set: "
                      << e.what() << std::endl;
        }
    }

    // Setup the EventLogger object for the provided session path
    session_proxy->LogForward(true);
    eventlogger = EventLogger::Create(mainloop, dbuscon, path);
}

} // namespace ovpn3cli::log
