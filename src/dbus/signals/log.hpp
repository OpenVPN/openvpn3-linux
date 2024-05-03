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
    DBus::Signals::Target::Ptr target = nullptr;
};


/**
 *  Provides a simpler interface to setup and process incoming
 *  Log D-Bus signals
 *
 *  This will call a lambda function providing each Log signal as a
 *  Events::Log() object accessible directly.
 */
class ReceiveLog
{
  public:
    using Ptr = std::shared_ptr<ReceiveLog>;
    using LogCallback = std::function<void(Events::Log)>;

    /**
     *  Prepares the Log event handler
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
     * @return ReceiveLog::Ptr handling this particular subscription.  When
     *         deleted, this object will unsubscribe from the Log signal
     */
    [[nodiscard]] static Ptr Create(DBus::Signals::SubscriptionManager::Ptr subscr,
                                    DBus::Signals::Target::Ptr subscr_tgt,
                                    LogCallback callback);
    ~ReceiveLog() noexcept;


  protected:
    ReceiveLog(DBus::Signals::SubscriptionManager::Ptr subscr,
               DBus::Signals::Target::Ptr subscr_tgt,
               LogCallback callback);

  private:
    DBus::Signals::SubscriptionManager::Ptr subscriptionmgr = nullptr;
    DBus::Signals::Target::Ptr target = nullptr;
    LogCallback log_callback{};
};

} // namespace Signals
