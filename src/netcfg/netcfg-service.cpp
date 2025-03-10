//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2021-  Heiko Hund <heiko@openvpn.net>
//

/**
 * @file   netcfg-service.cpp
 *
 * @brief  FIXME:
 */

#include <gdbuspp/connection.hpp>

// This needs to be included before any of the OpenVPN 3 Core
// header files.  This is needed to enable the D-Bus logging
// infrastructure for the Core library
#include "log/core-dbus-logger.hpp"

#include "netcfg-dco.hpp"
#include "netcfg-service.hpp"
#include "netcfg-service-handler.hpp"


NetCfgService::NetCfgService(DBus::Connection::Ptr dbuscon,
                             DNS::SettingsManager::Ptr resolver_,
                             LogWriter *logwr_,
                             NetCfgOptions options_)
    : DBus::Service(dbuscon, Constants::GenServiceName("netcfg")),
      resolver(resolver_),
      logwr(logwr_),
      options(options_)
{
    // Setup a signal object of the backend
    signals = NetCfgSignals::Create(GetConnection(),
                                    LogGroup::NETCFG,
                                    Constants::GenPath("netcfg"),
                                    logwr);
    signals->SetLogLevel(default_log_level);

    CreateServiceHandler<NetCfgServiceHandler>(GetConnection(),
                                               default_log_level,
                                               resolver,
                                               GetObjectManager(),
                                               logwr,
                                               options);

    // Log which redirect method is in use
    signals->LogVerb1(options.str());
}


void NetCfgService::SetDefaultLogLevel(unsigned int lvl)
{
    default_log_level = lvl;
}


void NetCfgService::BusNameAcquired(const std::string &busname)
{
    signals->Debug("NetCfg service registered on '" + busname
                   + "': " + Constants::GenPath("netcfg"));
    if (resolver)
    {
        signals->LogVerb2(resolver->GetBackendInfo());

        // Do a ApplySettings() call now, which will only fetch the
        // current system settings and initialize the resolver backend
        resolver->ApplySettings(signals);
    }
};

void NetCfgService::BusNameLost(const std::string &busname)
{
    throw DBus::Service::Exception(
        "openvpn3-service-netcfg lost the '"
        + busname + "' registration on the D-Bus");
};
