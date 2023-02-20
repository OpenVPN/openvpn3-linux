//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-signals.hpp
 *
 * @brief  D-Bus signals the net.openvpn.v3.netcfg can send
 */

#pragma once

#include "dbus/core.hpp"
#include "log/dbus-log.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-subscriptions.hpp"


class NetCfgSignals : public LogSender,
                      public RC<thread_unsafe_refcount>
{
  public:
    typedef RCPtr<NetCfgSignals> Ptr;

    NetCfgSignals(GDBusConnection *conn,
                  LogGroup lgroup,
                  std::string object_path,
                  LogWriter *logwr)
        : LogSender(conn, lgroup, OpenVPN3DBus_interf_netcfg, object_path, logwr)
    {
        SetLogLevel(default_log_level);
    }


    /**
     * Sends a FATAL log messages and kills itself
     *
     * @param Log message to send to the log subscribers
     */
    void LogFATAL(std::string msg) override
    {
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        kill(getpid(), SIGTERM);
    }


    void Debug(std::string msg, bool duplicate_check = false) override
    {
        Log(LogEvent(log_group, LogCategory::DEBUG, msg));
    }


    void Debug(std::string dev, std::string msg)
    {
        std::stringstream m;
        m << "[" << dev << "] " << msg;
        Debug(m.str());
    }


    void AddSubscriptionList(NetCfgSubscriptions::Ptr subs)
    {
        subscriptions = subs;
    }


    void NetworkChange(const NetCfgChangeEvent &ev) const
    {
        GVariant *e = ev.GetGVariant();
        if (subscriptions)
        {
            Send(subscriptions->GetSubscribersList(ev),
                 get_interface(),
                 get_object_path(),
                 "NetworkChange",
                 e);
        }
        else
        {
            // If no subscription manager is configured, we switch
            // to broadcasting NetworkChange signals.  This is typically
            // not configured if --signal-broadcast is given to the
            // main program.
            Send("NetworkChange", e);
        }
    }


  private:
    const unsigned int default_log_level = 6; // LogCategory::DEBUG
    NetCfgSubscriptions::Ptr subscriptions;
};
