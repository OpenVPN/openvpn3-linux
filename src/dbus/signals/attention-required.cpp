//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file dbus/signals/attention-required.cpp
 *
 * @brief C++ class used to send AttentionRequired signals
 *
 */

#include <iostream>
#include "attention-required.hpp"


namespace Signals {

AttentionRequired::AttentionRequired(DBus::Signals::Emit::Ptr emitter,
                                     DBus::Signals::SubscriptionManager::Ptr subscr,
                                     DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "AttentionRequired"),
      subscr_mgr(subscr), target(subscr_tgt)
{
    SetArguments(Events::AttentionReq::SignalDeclaration());
    Subscribe(subscr_tgt);
}


void AttentionRequired::Subscribe(DBus::Signals::Target::Ptr subscr_tgt)
{
    // If a SubscriptionManager and Signals::Target object is provided,
    // prepare proxying incoming AttentionRequired signals from that target
    if (subscr_mgr && subscr_tgt)
    {
        if (target)
        {
            subscr_mgr->Unsubscribe(target, "AttentionRequired");
        }

        subscr_mgr->Subscribe(subscr_tgt,
                              "AttentionRequired",
                              [this](DBus::Signals::Event::Ptr event)
                              {
                                  try
                                  {
                                      Events::AttentionReq ev(event->params);
                                      (void)Send(ev);
                                  }
                                  catch (const DBus::Exception &ex)
                                  {
                                      std::cerr << "AttentionReq EXCEPTION:"
                                                << ex.what() << std::endl;
                                  }
                              });

        target = subscr_tgt;
    }
}


bool AttentionRequired::Send(Events::AttentionReq &event) const
{
    try
    {
        return EmitSignal(event.GetGVariant());
    }
    catch (const DBus::Signals::Exception &ex)
    {
        std::cerr << "AttentionReq::Send() EXCEPTION:"
                  << ex.what() << std::endl;
    }
    return false;
}


bool AttentionRequired::Send(const ClientAttentionType &type,
                             const ClientAttentionGroup &group,
                             const std::string &msg) const
{
    return EmitSignal(g_variant_new(GetDBusType(),
                                    static_cast<uint32_t>(type),
                                    static_cast<uint32_t>(group),
                                    msg.c_str()));
}

} // namespace Signals
