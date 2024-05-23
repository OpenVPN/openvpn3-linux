//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log/event-logger.hpp
 *
 * @brief  Helper class to process incoming Log and StatusChange signals
 *         for a session
 */

#pragma once

#include <fstream>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "dbus/signals/log.hpp"
#include "dbus/signals/statuschange.hpp"
#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"


namespace ovpn3cli::log {

/**
 *  This object will receive all the Log and StatusChange signals for a
 *  specific VPN session.  It will write these events to the console/terminal
 *  as they appear.
 *
 *  If the session is closed, even will trigger the main loop this object
 *  runs under to stop as well.
 */
class EventLogger
{
  public:
    using Ptr = std::shared_ptr<EventLogger>;

    /**
     *  Create the EventLogger object
     *
     * @param mainloop      DBus::MainLoop object this runs under
     * @param dbusconn      DBus::Connection to use for signal subscription
     * @param session_path  DBus::Object::Path to the session to subscribe to
     * @return EventLogger::Ptr (aka std::shared_ptr<EventLogger>
     */
    [[nodiscard]] static Ptr Create(DBus::MainLoop::Ptr mainloop,
                                    DBus::Connection::Ptr dbusconn,
                                    const DBus::Object::Path &session_path);

  private:
    DBus::MainLoop::Ptr mainloop = nullptr;
    std::ostream logstream;
    LogWriter::Ptr logdest = nullptr;
    ColourEngine::Ptr colourengine = nullptr;
    DBus::Signals::SubscriptionManager::Ptr submgr = nullptr;
    DBus::Signals::Target::Ptr signal_sender = nullptr;
    ::Signals::ReceiveLog::Ptr sighandler_log = nullptr;
    ::Signals::ReceiveStatusChange::Ptr sighandler_statuschg = nullptr;

    EventLogger(DBus::MainLoop::Ptr mainl,
                DBus::Connection::Ptr conn,
                const DBus::Object::Path &session_path);
};

} // namespace ovpn3cli::log
