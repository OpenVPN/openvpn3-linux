//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log/event-logger.cpp
 *
 * @brief  Helper class to process incoming Log and StatusChange signals
 *         for a session
 */


#include "common/utils.hpp"
#include "event-logger.hpp"
#include "log/ansicolours.hpp"


namespace ovpn3cli::log {

EventLogger::Ptr EventLogger::Create(DBus::MainLoop::Ptr mainloop,
                                     DBus::Connection::Ptr dbuscon,
                                     const DBus::Object::Path &session_path)

{
    return Ptr(new EventLogger(mainloop, dbuscon, session_path));
}


EventLogger::EventLogger(DBus::MainLoop::Ptr mainl,
                         DBus::Connection::Ptr conn,
                         const DBus::Object::Path &session_path)
    : mainloop(mainl),
      logstream(std::cout.rdbuf())
{
    if (is_colour_terminal())
    {
        colourengine = std::make_unique<ANSIColours>();
        logdest = std::make_shared<ColourStreamWriter>(logstream,
                                                       colourengine.get());
    }
    else
    {
        logdest = std::make_shared<StreamLogWriter>(logstream);
    }

    auto prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
    auto log_busname = prxqry->GetNameOwner(Constants::GenServiceName("log"));

    submgr = DBus::Signals::SubscriptionManager::Create(conn);
    signal_sender = DBus::Signals::Target::Create(log_busname, session_path, "");

    sighandler_log = ::Signals::ReceiveLog::Create(
        submgr,
        signal_sender,
        [&](const Events::Log logev)
        {
            Events::Log event(logev);
            event.message = logev.str(23);
            logdest->Write(event);
        });

    sighandler_statuschg = ::Signals::ReceiveStatusChange::Create(
        submgr,
        signal_sender,
        [&](const std::string &sender, const DBus::Object::Path &path, const std::string &interface, Events::Status stchgev)
        {
            logdest->WriteStatus(stchgev);
        });
}

} // namespace ovpn3cli::log
