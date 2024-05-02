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


#include "statuschange.hpp"


namespace Signals {

StatusChange::StatusChange(DBus::Signals::Emit::Ptr emitter,
                           DBus::Signals::SubscriptionManager::Ptr subscr,
                           DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "StatusChange"),
      target(subscr_tgt)
{
    SetArguments(Events::Status::SignalDeclaration());

    // Prepare proxying incoming StatusChange signals
    if (subscr && subscr_tgt)
    {
        subscr->Subscribe(target,
                          "StatusChange",
                          [=](DBus::Signals::Event::Ptr event)
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


GVariant *StatusChange::LastStatusChange() const
{
    return last_ev.GetGVariantTuple();
}

}
