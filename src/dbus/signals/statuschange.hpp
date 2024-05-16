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

/**
 *  Provides a simpler interface to setup and process incoming
 *  StatusChange D-Bus signals
 *
 *  This will call a lambda function providing each Log signal as a
 *  Events::Status() object accessible directly.
 */
class ReceiveStatusChange
{
  public:
    using Ptr = std::shared_ptr<ReceiveStatusChange>;
    using StatusChgCallback = std::function<void(const std::string &sender,
                                                 const DBus::Object::Path &path,
                                                 const std::string &interface,
                                                 Events::Status)>;

    /**
     *  Prepares the StatusChange event handler
     *
     *  This requires a SubscriptionManager and a Signals::Target object
     *  for the signal subscription.
     *
     * @param subscr       DBus::Signals::SubscriptionManager managing the
     *                     D-Bus signal subscriptions
     * @param subscr_tgt   DBus::Signals::Target containing the subscription
     *                     details.  May be empty strings, if a more broader
     *                     subscription
     * @param callback     Lambda function being called each time a Log
     *                     signal is received
     * @return ReceiveStatusChange::Ptr handling this particular subscription.
     *         When deleted, this object will unsubscribe from the
     *         StatusChange signal
     */
    [[nodiscard]] static Ptr Create(DBus::Signals::SubscriptionManager::Ptr subscr,
                                    DBus::Signals::Target::Ptr subscr_tgt,
                                    StatusChgCallback callback);
    ~ReceiveStatusChange() noexcept;


  protected:
    ReceiveStatusChange(DBus::Signals::SubscriptionManager::Ptr subscr,
                        DBus::Signals::Target::Ptr subscr_tgt,
                        StatusChgCallback callback);

  private:
    DBus::Signals::SubscriptionManager::Ptr subscriptionmgr = nullptr;
    DBus::Signals::Target::Ptr target = nullptr;
    StatusChgCallback statuschg_callback{};
};

} // namespace Signals
