//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file sessionmgr-signals.hpp
 *
 * @brief Provides helper classes handling StatusChange D-Bus signals
 */

#pragma once

#include <memory>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/status.hpp"


namespace Signals {


/**
 *  Provides an implementation to send the net.openvpn.v3.*.StatusChange signal
 *
 *  This class implements a signal proxy, subscribing to the same
 *  signal from the net.openvpn.v3.backends.be* client backend service
 *  and forwarding it to the user front-ends needing to respond to
 *  such events.  This proxying will only be enabled if a SubscriptionManager
 *  and Signals::Target objects are provided.
 */
class StatusChange : public DBus::Signals::Signal
{
  public:
    using Ptr = std::shared_ptr<StatusChange>;

    StatusChange(DBus::Signals::Emit::Ptr emitter,
                 DBus::Signals::SubscriptionManager::Ptr subscr = nullptr,
                 DBus::Signals::Target::Ptr subscr_tgt = nullptr);
    ~StatusChange() noexcept = default;

    const std::string GetSignature() const;

    bool Send(const Events::Status &stch) noexcept;
    GVariant *LastStatusChange() const;

  private:
    Events::Status last_ev{};
    DBus::Signals::Target::Ptr target{};
};

}
