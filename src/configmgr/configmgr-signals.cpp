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
 * @file configmgr-signals.cpp
 *
 * @brief Implementation of helper classes handling D-Bus signals for
 *        net.openvpn.v3.configuration
 */

#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "configmgr-exceptions.hpp"
#include "configmgr-signals.hpp"
#include "constants.hpp"


namespace ConfigManager {

Log::Ptr Log::Create(DBus::Connection::Ptr conn,
                     LogGroup lgroup,
                     const std::string &object_path,
                     LogWriter::Ptr logwr)
{
    return Log::Ptr(new Log(conn, lgroup, object_path, logwr));
}


void Log::LogFATAL(const std::string &msg)
{
    LogSender::Log(Events::Log(log_group, LogCategory::FATAL, msg));
    abort();
}


Log::Log(DBus::Connection::Ptr conn,
         LogGroup lgroup,
         const std::string &object_path_,
         LogWriter::Ptr logwr)
    : LogSender(conn, lgroup, object_path_, INTERFACE_CONFIGMGR, false, logwr.get())
{
    auto creds = DBus::Credentials::Query::Create(conn);

    auto srvqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
    if (!srvqry->CheckServiceAvail(Constants::GenServiceName("log")))
    {
        throw ConfigManager::Exception("Could not connect to log service");
    }

    AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

    SetLogLevel(default_log_level);
}

} // namespace ConfigManager


namespace Signals {

ConfigurationManagerEvent::ConfigurationManagerEvent(DBus::Signals::Emit::Ptr emitter)
    : DBus::Signals::Signal(emitter, "ConfigurationManagerEvent")
{
    SetArguments(ConfigManager::Event::SignalDeclaration());
}


bool ConfigurationManagerEvent::Send(const std::string &path,
                                     ConfigManager::EventType type,
                                     uid_t owner) const noexcept
{
    try
    {
        ConfigManager::Event ev(path, type, owner);
        return EmitSignal(ev.GetGVariant());
    }
    catch (const DBus::Signals::Exception &ex)
    {
        std::cerr << "ConfigurationManagerEvent::Send() EXCEPTION:"
                  << ex.what() << std::endl;
    }
    return false;
}

} // namespace Signals
