//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file sessionmgr-signals.cpp
 *
 * @brief Provides helper classes handling StatusChange D-Bus signals
 */


#include <iostream>
#include "statuschange.hpp"


namespace Signals {

StatusChange::StatusChange(DBus::Signals::Emit::Ptr emitter,
                           DBus::Signals::SubscriptionManager::Ptr subscr,
                           DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "StatusChange"),
      target(subscr_tgt), subscr_mgr(subscr)
{
    SetArguments(Events::Status::SignalDeclaration());
    Subscribe(subscr_tgt);
}


void StatusChange::Subscribe(DBus::Signals::Target::Ptr subscr_tgt)
{
    // Prepare proxying incoming StatusChange signals
    if (subscr_mgr && subscr_tgt)
    {
        if (target)
        {
            subscr_mgr->Unsubscribe(target, "StatusChange");
        }

        subscr_mgr->Subscribe(subscr_tgt,
                              "StatusChange",
                              [this](DBus::Signals::Event::Ptr event)
                              {
                                  try
                                  {
                                      Events::Status ev(event->params);
                                      (void)Send(ev);
                                  }
                                  catch (const DBus::Exception &ex)
                                  {
                                      std::cerr << "StatusChange EXCEPTION:"
                                                << ex.what() << std::endl;
                                  }
                              });

        target = subscr_tgt;
    }
}


const std::string StatusChange::GetSignature() const
{
    return DBus::Signals::SignalArgSignature(Events::Status::SignalDeclaration());
}


bool StatusChange::Send(const Events::Status &stch) noexcept
{
    last_ev = stch;
    try
    {
        return EmitSignal(stch.GetGVariantTuple());
    }
    catch (const DBus::Signals::Exception &ex)
    {
        std::cerr << "StatusChange::Send() EXCEPTION:"
                  << ex.what() << std::endl;
    }
    return false;
}


Events::Status StatusChange::LastEvent() const
{
    return last_ev;
}

GVariant *StatusChange::LastStatusChange() const
{
    if (last_ev.empty())
    {
        // Nothing have been logged, nothing to report
        Events::Status empty;
        return empty.GetGVariantTuple();
    }

    return last_ev.GetGVariantTuple();
}



ReceiveStatusChange::Ptr ReceiveStatusChange::Create(DBus::Signals::SubscriptionManager::Ptr subscr,
                                                     DBus::Signals::Target::Ptr subscr_tgt,
                                                     StatusChgCallback callback)
{
    return ReceiveStatusChange::Ptr(new ReceiveStatusChange(subscr,
                                                            subscr_tgt,
                                                            std::move(callback)));
}


ReceiveStatusChange::ReceiveStatusChange(DBus::Signals::SubscriptionManager::Ptr subscr,
                                         DBus::Signals::Target::Ptr subscr_tgt,
                                         StatusChgCallback callback)
    : subscriptionmgr(subscr), target(subscr_tgt),
      statuschg_callback(callback)
{
    if (!subscriptionmgr || !target)
    {
        throw DBus::Signals::Exception("Undefined subscription manager or target");
    }

    subscriptionmgr->Subscribe(
        target,
        "StatusChange",
        [&](DBus::Signals::Event::Ptr event)
        {
            GVariant *params = event->params;
            auto stchgev = Events::Status(params);
            statuschg_callback(event->sender,
                               event->object_path,
                               event->object_interface,
                               std::move(stchgev));
        });
}


ReceiveStatusChange::~ReceiveStatusChange() noexcept
{
    subscriptionmgr->Unsubscribe(target, "StatusChange");
}

} // namespace Signals
