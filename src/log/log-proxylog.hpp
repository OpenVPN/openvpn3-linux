//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file log-proxylog.hpp
 *
 *  @brief Declares LogService::ProxyLogEvents object, created via the
 *         the net.openvpn.v3.log.ProxyLogEvents D-Bus method
 */

#pragma once

#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/manager.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/target.hpp>

#include "dbus/constants.hpp"
#include "dbus/path.hpp"
#include "dbus/signals/log.hpp"
#include "dbus/signals/statuschange.hpp"
#include "events/log.hpp"
#include "events/status.hpp"
#include "log/logfilter.hpp"
#include "service-logger.hpp"


namespace LogService {

class ProxyLogSignals : public DBus::Signals::Group
{
  public:
    using Ptr = std::shared_ptr<ProxyLogSignals>;
    const DBus::Object::Path session_path;

    ProxyLogSignals(DBus::Connection::Ptr conn,
                    const DBus::Object::Path &path,
                    const std::string &interf);

    void SendLog(const Events::Log &logev) const;
    void SendStatusChange(const Events::Status &stchgev) const;

  private:
    Signals::Log::Ptr signal_log = nullptr;
    Signals::StatusChange::Ptr signal_statuschg = nullptr;
};



/**
 *  This is object is providing the D-Bus object for a specific proxy log
 *  request.  This object will only provide an API to _send_ Log and
 *  StatusChange signals.  This API will be called via the AttachedService
 *  object which receives these signals for the general service logging.
 */
class ProxyLogEvents : public DBus::Object::Base
{
  public:
    // using Ptr = std::shared_ptr<ProxyLogEvents>;

    ProxyLogEvents(DBus::Connection::Ptr connection_,
                   DBus::Object::Manager::Ptr obj_mgr,
                   const std::string &recv_tgt,
                   const DBus::Object::Path &session_objpath,
                   const std::string &session_interf,
                   const uint32_t init_loglev);
    virtual ~ProxyLogEvents() noexcept = default;

    std::string GetReceiverTarget() const noexcept;

    void SendLog(const Events::Log &logev) const;
    void SendStatusChange(const DBus::Object::Path &path,
                          const Events::Status &stchgev) const;

    const bool Authorize(const DBus::Authz::Request::Ptr req) override;
    const std::string AuthorizationRejected(const DBus::Authz::Request::Ptr req) const noexcept override;

  private:
    DBus::Connection::Ptr connection = nullptr;
    DBus::Object::Manager::Ptr object_mgr = nullptr;
    Log::EventFilter::Ptr filter = nullptr;
    DBus::Object::Path session_path = {};
    const std::string receiver_target;
    uid_t target_uid;
    DBus::Credentials::Query::Ptr credsqry = nullptr;
    ProxyLogSignals::Ptr signal_proxy = nullptr;
};

} // namespace LogService
