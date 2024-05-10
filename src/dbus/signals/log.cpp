//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file dbus/signals/log.cpp
 *
 * @brief Provides helper classes handling Log D-Bus signals
 */

#include <gdbuspp/signals/event.hpp>

#include "log.hpp"


namespace Signals {

Log::Log(DBus::Signals::Emit::Ptr emitter,
         DBus::Signals::SubscriptionManager::Ptr subscr,
         DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "Log"),
      target(subscr_tgt)
{
    SetArguments(Events::Log::SignalDeclaration());

    // Prepare proxying incoming StatusChange signals
    if (subscr && subscr_tgt)
    {
        subscr->Subscribe(target,
                          "Log",
                          [=](DBus::Signals::Event::Ptr event)
                          {
                              try
                              {
                                  Events::Log ev(event->params);
                                  (void)Send(ev);
                              }
                              catch (const DBus::Exception &ex)
                              {
                                  std::cerr << "Log EXCEPTION:"
                                            << ex.what() << std::endl;
                              }
                          });
    }
}


const std::string Log::GetSignature() const
{
    return DBus::Signals::SignalArgSignature(Events::Log::SignalDeclaration());
}


bool Log::Send(const Events::Log &logev) noexcept
{
    last_ev = logev;
    try
    {
        return EmitSignal(logev.GetGVariantTuple());
    }
    catch (const DBus::Signals::Exception &ex)
    {
        std::cerr << "Log::Send() EXCEPTION:"
                  << ex.what() << std::endl;
    }
    return false;
}


GVariant *Log::LastLogEvent() const
{
    return last_ev.GetGVariantTuple();
}


ReceiveLog::Ptr ReceiveLog::Create(DBus::Signals::SubscriptionManager::Ptr subscr,
                                   DBus::Signals::Target::Ptr subscr_tgt,
                                   LogCallback callback)
{
    return ReceiveLog::Ptr(new ReceiveLog(subscr,
                                          subscr_tgt,
                                          std::move(callback)));
}


ReceiveLog::ReceiveLog(DBus::Signals::SubscriptionManager::Ptr subscr,
                       DBus::Signals::Target::Ptr subscr_tgt,
                       LogCallback callback)
    : subscriptionmgr(subscr), target(subscr_tgt),
      log_callback(callback)
{
    if (!subscriptionmgr || !target)
    {
        throw DBus::Signals::Exception("Undefined subscription manager or target");
    }

    subscriptionmgr->Subscribe(
        target,
        "Log",
        [&](DBus::Signals::Event::Ptr event)
        {
            GVariant *params = event->params;
            auto sender = DBus::Signals::Target::Create(event->sender,
                                                        event->object_path,
                                                        event->object_interface);
            auto logev = Events::Log(params, std::move(sender));
            log_callback(std::move(logev));
        });
}


ReceiveLog::~ReceiveLog() noexcept
{
    subscriptionmgr->Unsubscribe(target, "Log");
}

} // namespace Signals
