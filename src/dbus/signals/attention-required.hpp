//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file dbus/signals/attention-required.hpp
 *
 * @brief C++ class used to send AttentionRequired signals
 *
 */

#pragma once

#include <memory>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/attention-req.hpp"


namespace Signals {

/**
 *  Provides an implementation to send and proxy the
 *  net.openvpn.v3.*.AttentionRequired signal.
 *
 *  This class implements a signal proxy, subscribing to the same
 *  signal from the net.openvpn.v3.backends.be* client backend service
 *  and forwarding it to the user front-ends needing to respond to
 *  such events.  This proxying will only be enabled if a SubscriptionManager
 *  and Signals::Target objects are provided.
 */
class AttentionRequired : public DBus::Signals::Signal
{
  public:
    using Ptr = std::shared_ptr<AttentionRequired>;

    /**
     *  Prepare the AttentionRequired signal emitter class
     *
     * @param emitter     DBus::Signals::Emit object used to emit D-Bus signals
     * @param subscr      (optional) DBus::Signals::SubscriptionManager object
     *                    handling D-Bus signal subscriptions. Needed when
     *                    listening for incoming AttentionRequired signals
     *                    needed to be proxied further
     * @param subscr_tgt  (optional) DBus::Signals::Target object with the
     *                    containing AttentionRequired subscription details.
     *                    Used to listen for signals from specific sender
     *                    targets.
     */
    AttentionRequired(DBus::Signals::Emit::Ptr emitter,
                      DBus::Signals::SubscriptionManager::Ptr subscr = nullptr,
                      DBus::Signals::Target::Ptr subscr_tgt = nullptr);
    ~AttentionRequired() noexcept = default;

    void Subscribe(DBus::Signals::Target::Ptr subscr_tgt);

    bool Send(Events::AttentionReq &event) const;
    bool Send(const ClientAttentionType &type,
              const ClientAttentionGroup &group,
              const std::string &msg) const;

  private:
    DBus::Signals::SubscriptionManager::Ptr subscr_mgr;
    DBus::Signals::Target::Ptr target;
};

} // namespace Signals
