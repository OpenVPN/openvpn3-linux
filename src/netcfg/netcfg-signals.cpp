//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-signals.cpp
 *
 * @brief  Implementation of the NetCfgSignals class
 */

#include <signal.h>
#include <sstream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/group.hpp>

#include "log/dbus-log.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-signals.hpp"
#include "netcfg-subscriptions.hpp"



NetCfgSignals::NetCfgSignals(DBus::Connection::Ptr conn,
                             LogGroup lgroup,
                             std::string object_path_,
                             LogWriter *logwr)
    : LogSender(conn,
                lgroup,
                object_path_,
                Constants::GenInterface("netcfg"),
                false,
                logwr),
      object_path(object_path_),
      object_interface(Constants::GenInterface("netcfg"))
{
    auto creds = DBus::Credentials::Query::Create(conn);
    AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

    RegisterSignal("NetworkChange", NetCfgChangeEvent::SignalDeclaration());

    SetLogLevel(default_log_level);
    GroupCreate(object_path);
}


/**
 * Sends a FATAL log messages and kills itself
 *
 * @param Log message to send to the log subscribers
 */
void NetCfgSignals::LogFATAL(const std::string &msg)
{
    Log(LogEvent(log_group, LogCategory::FATAL, msg));
    kill(getpid(), SIGTERM);
}


void NetCfgSignals::Debug(const std::string &msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::DEBUG, msg));
}


void NetCfgSignals::DebugDevice(const std::string &dev, const std::string &msg)
{
    std::stringstream m;
    m << "[" << dev << "] " << msg;
    Debug(m.str());
}


void NetCfgSignals::AddSubscriptionList(NetCfgSubscriptions::Ptr subs)
{
    subscriptions = subs;
}


void NetCfgSignals::NetworkChange(const NetCfgChangeEvent &ev)
{
    GVariant *e = ev.GetGVariant();
    if (subscriptions)
    {
        GroupAddTargetList(object_path,
                           subscriptions->GetSubscribersList(ev));
        GroupSendGVariant(object_path, "NetworkChange", e);
        GroupClearTargets(object_path);
    }
    else
    {
        // If no subscription manager is configured, we switch
        // to broadcasting NetworkChange signals.
        SendGVariant("NetworkChange", e);
    }
}
