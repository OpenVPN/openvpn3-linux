//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file dbus/signals/log.hpp
 *
 * @brief Provides helper classes handling Log D-Bus signals
 */

#pragma once

#include <memory>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/log.hpp"


namespace Signals {

/**
 *  Provides an implementation to send the net.openvpn.v3.*.Log signal
 *
 *  This class implements a signal proxy, subscribing to the same
 *  signal.  This proxying will only be enabled if a SubscriptionManager
 *  and Signals::Target objects are provided.
 */
class Log : public DBus::Signals::Signal
{
  public:
    using Ptr = std::shared_ptr<Log>;

    Log(DBus::Signals::Emit::Ptr emitter,
        DBus::Signals::SubscriptionManager::Ptr subscr = nullptr,
        DBus::Signals::Target::Ptr subscr_tgt = nullptr);
    ~Log() noexcept = default;

    const std::string GetSignature() const;

    bool Send(const Events::Log &logev) noexcept;
    GVariant *LastLogEvent() const;

  private:
    Events::Log last_ev{};
    DBus::Signals::Target::Ptr target{};
};

} // namespace Signals
