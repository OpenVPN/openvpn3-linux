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
 * @file   netcfg.hpp
 *
 * @brief  The implementation of the net.openvpn.v3.netcfg D-Bus service
 */

#pragma once

#include <map>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/service.hpp>

#include "log/logwriter.hpp"
#include "netcfg-options.hpp"
#include "dns/settings-manager.hpp"

using namespace NetCfg;

class NetCfgServiceHandler;

class NetCfgService : public DBus::Service
{
  public:
    /**
     *  Initializes the NetworkCfgService object
     *
     * @param bus_type   GBusType, which defines if this service should be
     *                   registered on the system or session bus.
     * @param logwr      LogWriter object which takes care of the log processing
     *
     */
    NetCfgService(DBus::Connection::Ptr dbuscon,
                  DNS::SettingsManager::Ptr resolver,
                  LogWriter *logwr,
                  NetCfgOptions options);
    ~NetCfgService() = default;

    void BusNameAcquired(const std::string &busname) override;

    void BusNameLost(const std::string &busname) override;

  private:
    DNS::SettingsManager::Ptr resolver = nullptr;
    LogWriter *logwr = nullptr;

    NetCfgSignals::Ptr signals = nullptr;
    NetCfgOptions options;
};
