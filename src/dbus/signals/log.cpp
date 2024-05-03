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

} // namespace Signals
