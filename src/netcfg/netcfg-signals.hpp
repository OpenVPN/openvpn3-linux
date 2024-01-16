//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-signals.hpp
 *
 * @brief  D-Bus signals the net.openvpn.v3.netcfg service can send
 */

#pragma once

#include <memory>
#include <gdbuspp/connection.hpp>

#include "log/dbus-log.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-subscriptions.hpp"


class NetCfgSignals : public LogSender
{
  public:
    using Ptr = std::shared_ptr<NetCfgSignals>;

    [[nodiscard]] static NetCfgSignals::Ptr Create(DBus::Connection::Ptr conn,
                                                   LogGroup lgroup,
                                                   std::string object_path,
                                                   LogWriter *logwr)
    {
        return NetCfgSignals::Ptr(new NetCfgSignals(conn,
                                                    lgroup,
                                                    object_path,
                                                    logwr));
    }

    /**
     * Sends a FATAL log messages and kills itself
     *
     * @param Log message to send to the log subscribers
     */
    void LogFATAL(const std::string &msg) override;

    void Debug(const std::string &msg, bool duplicate_check = false) override;

    void DebugDevice(const std::string &dev, const std::string &msg);

    void AddSubscriptionList(NetCfgSubscriptions::Ptr subs);

    void NetworkChange(const NetCfgChangeEvent &ev);


  private:
    const unsigned int default_log_level = 6; // LogCategory::DEBUG
    NetCfgSubscriptions::Ptr subscriptions;
    const std::string object_path;
    const std::string object_interface;


    NetCfgSignals(DBus::Connection::Ptr conn,
                  LogGroup lgroup,
                  std::string object_path_,
                  LogWriter *logwr);
};
