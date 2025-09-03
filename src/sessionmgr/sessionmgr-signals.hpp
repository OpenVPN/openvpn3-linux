//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018-  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file sessionmgr-signals.hpp
 *
 * @brief Declaration helper classes handling D-Bus signals for
 *        net.openvpn.v3.sessions
 */

#pragma once
#include <memory>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/attention-req.hpp"
#include "events/status.hpp"
#include "log/dbus-log.hpp"
#include "sessionmgr-events.hpp"


namespace SessionManager {

/**
 * Helper class to tackle signals sent by the session manager
 *
 * This mostly just wraps the LogSender class and predefines LogGroup to always
 * be SESSIONMGR.
 */
class Log : public LogSender
{
  public:
    using Ptr = std::shared_ptr<Log>;

    [[nodiscard]] static Log::Ptr Create(DBus::Connection::Ptr conn,
                                         LogGroup lgroup,
                                         const std::string &object_path,
                                         LogWriter::Ptr logwr);
    ~Log() = default;

    /**
     *  Whenever a FATAL error happens, the process is expected to stop.
     *  The abort() call gets caught by the main loop, which then starts the
     *  proper shutdown process.
     *
     * @param msg  Message to sent to the log subscribers
     */
    void LogFATAL(const std::string &msg) override;

  private:
    const std::string object_path;
    const std::string object_interface;
    uint8_t default_log_level = 6;

    Log(DBus::Connection::Ptr conn,
        LogGroup lgroup,
        const std::string &object_path_,
        LogWriter::Ptr logwr);
};

} // namespace SessionManager


namespace Signals {

/**
 *  Provides an implementation to send the
 *  net.openvpn.v3.sessions.SessionManagerEvent signal.
 *
 * This is a fairly simple signal, only emitted by the Session Manager
 * when a new session is created or destroyed.
 *
 */
class SessionManagerEvent : public DBus::Signals::Signal
{
  public:
    using Ptr = std::shared_ptr<SessionManagerEvent>;

    SessionManagerEvent(DBus::Signals::Emit::Ptr emitter);

    bool Send(const std::string &path,
              SessionManager::EventType type,
              uid_t owner) const noexcept;
};

} // namespace Signals
