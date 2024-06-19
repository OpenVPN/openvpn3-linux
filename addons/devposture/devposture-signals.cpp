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
 * @file devposture-signals.cpp
 *
 * @brief Implementation of helper classes handling D-Bus signals for
 *        net.openvpn.v3.devposture
 */

#include <memory>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "devposture-signals.hpp"
#include "constants.hpp"


namespace DevPosture {

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
    : LogSender(conn, lgroup, object_path_, INTERFACE_DEVPOSTURE, false, logwr.get()),
      object_path(object_path_), object_interface(INTERFACE_DEVPOSTURE)
{
    auto creds = DBus::Credentials::Query::Create(conn);
    AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

    SetLogLevel(default_log_level);
}

} // namespace DevPosture
