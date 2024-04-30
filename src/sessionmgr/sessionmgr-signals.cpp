//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018-  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file sessionmgr-signals.cpp
 *
 * @brief Implementation of helper classes handling D-Bus signals for
 *        net.openvpn.v3.sessions
 */

#include <memory>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/signal.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "sessionmgr-signals.hpp"


namespace SessionManager {

Log::Ptr Log::Create(DBus::Connection::Ptr conn,
                     LogGroup lgroup,
                     const std::string &object_path,
                     LogWriter::Ptr logwr)
{
    return Log::Ptr(new Log(conn,
                            lgroup,
                            object_path,
                            logwr));
}


void Log::LogFATAL(const std::string &msg)
{
    LogSender::Log(Events::Log(log_group, LogCategory::FATAL, msg));
    StatusChange(Events::Status(StatusMajor::SESSION, StatusMinor::PROC_KILLED, msg));
    abort();
}


Log::Log(DBus::Connection::Ptr conn,
         LogGroup lgroup,
         const std::string &object_path_,
         LogWriter::Ptr logwr)
    : LogSender(conn,
                lgroup,
                object_path_,
                Constants::GenInterface("sessions"),
                false,
                logwr.get(),
                true),
      object_path(object_path_),
      object_interface(Constants::GenInterface("sessions"))
{
    auto creds = DBus::Credentials::Query::Create(conn);
    AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

    SetLogLevel(default_log_level);
}



namespace Signals {

AttentionRequired::AttentionRequired(DBus::Signals::Emit::Ptr emitter,
                                     DBus::Signals::SubscriptionManager::Ptr subscr,
                                     DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "AttentionRequired")
{
    SetArguments(Events::AttentionReq::SignalDeclaration());

    // Prepare proxying incoming AttentionRequired signals
    subscr->Subscribe(subscr_tgt,
                      "AttentionRequired",
                      [=](DBus::Signals::Event::Ptr event)
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
}


bool AttentionRequired::Send(Events::AttentionReq &event) const noexcept
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



StatusChange::StatusChange(DBus::Signals::Emit::Ptr emitter,
                           DBus::Signals::SubscriptionManager::Ptr subscr,
                           DBus::Signals::Target::Ptr subscr_tgt)
    : DBus::Signals::Signal(emitter, "StatusChange"),
      target(subscr_tgt)
{
    SetArguments(Events::Status::SignalDeclaration());

    // Prepare proxying incoming StatusChange signals
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



SessionManagerEvent::SessionManagerEvent(DBus::Signals::Emit::Ptr emitter)
    : DBus::Signals::Signal(emitter, "SessionManagerEvent")
{
    SetArguments(SessionManager::Event::SignalDeclaration());
}


bool SessionManagerEvent::Send(const std::string &path,
                               EventType type,
                               uid_t owner) const noexcept
{
    try
    {

        SessionManager::Event ev(path, type, owner);
        return EmitSignal(ev.GetGVariant());
    }
    catch (const DBus::Signals::Exception &ex)
    {
        std::cerr << "SessionManagerEvent::Send() EXCEPTION:"
                  << ex.what() << std::endl;
    }
    return false;
}


} // namespace Signals

} // namespace SessionManager
