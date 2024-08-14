//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 * @file configmgr-signals.hpp
 *
 * @brief Declaration helper classes handling D-Bus signals for
 *        net.openvpn.v3.configuration
 */

#pragma once

#include <memory>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "configmgr-events.hpp"
#include "log/dbus-log.hpp"

namespace ConfigManager {

/**
 * Helper class to tackle signals sent by the session manager
 *
 * This mostly just wraps the LogSender class and predefines LogGroup to always
 * be CONFIGMGR.
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
    uint8_t default_log_level = 6;

    Log(DBus::Connection::Ptr conn,
        LogGroup lgroup,
        const std::string &object_path_,
        LogWriter::Ptr logwr);
};

} // namespace ConfigManager


namespace Signals {

/**
 *  Provides an implementation to send the
 *  net.openvpn.v3.configuration.ConfigurationManagerEvent signal.
 *
 * This is a fairly simple signal, only emitted by the Configuration
 * Manager when a new configuration is created or destroyed.
 *
 */
class ConfigurationManagerEvent : public DBus::Signals::Signal
{
  public:
    using Ptr = std::shared_ptr<ConfigurationManagerEvent>;

    ConfigurationManagerEvent(DBus::Signals::Emit::Ptr emitter);

    bool Send(const std::string &path,
              ConfigManager::EventType type,
              uid_t owner) const noexcept;
};

} // namespace Signals
