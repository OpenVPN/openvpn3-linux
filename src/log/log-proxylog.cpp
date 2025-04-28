//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file log-proxylog.cpp
 *
 *  @brief Implements LogService::ProxyLogEvents and related objects
 */

#include "log-proxylog.hpp"

namespace LogService {

ProxyLogSignals::ProxyLogSignals(DBus::Connection::Ptr conn,
                                 const DBus::Object::Path &path,
                                 const std::string &interf)
    : DBus::Signals::Group(conn, path, interf),
      session_path(path)
{
    signal_log = CreateSignal<Signals::Log>();
    signal_statuschg = CreateSignal<Signals::StatusChange>();
}


void ProxyLogSignals::SendLog(const Events::Log &logev) const
{
    signal_log->Send(logev);
}

void ProxyLogSignals::SendStatusChange(const Events::Status &stchgev) const
{
    signal_statuschg->Send(stchgev);
}



ProxyLogEvents::ProxyLogEvents(DBus::Connection::Ptr connection_,
                               DBus::Object::Manager::Ptr obj_mgr,
                               LogService::Logger::Ptr log_,
                               const std::string &recv_tgt,
                               const DBus::Object::Path &session_objpath,
                               const std::string &session_interf,
                               const uint32_t init_loglev)
    : DBus::Object::Base(generate_path_uuid(Constants::GenPath("log/proxy"), 'l'),
                         Constants::GenInterface("log")),
      connection(connection_), object_mgr(obj_mgr), log(log_),
      filter(Log::EventFilter::Create(init_loglev)),
      session_path(session_objpath),
      receiver_target(recv_tgt)

{
    credsqry = DBus::Credentials::Query::Create(connection);
    target_uid = credsqry->GetUID(recv_tgt);
    signal_proxy = DBus::Signals::Group::Create<ProxyLogSignals>(connection_,
                                                                 session_objpath,
                                                                 session_interf);
    signal_proxy->AddTarget(receiver_target);

    AddMethod("Remove",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  object_mgr->RemoveObject(GetPath());
                  args->SetMethodReturn(nullptr);
              });

    AddPropertyBySpec(
        "log_level",
        glib2::DataType::DBus<uint32_t>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(filter->GetLogLevel());
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value)
            -> DBus::Object::Property::Update::Ptr
        {
            filter->SetLogLevel(glib2::Value::Get<uint32_t>(value));
            auto upd = prop.PrepareUpdate();
            upd->AddValue(filter->GetLogLevel());
            return upd;
        });

    AddPropertyBySpec(
        "session_path",
        glib2::DataType::DBus<DBus::Object::Path>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(signal_proxy->session_path);
        });

    AddPropertyBySpec(
        "target",
        glib2::DataType::DBus<DBus::Object::Path>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(receiver_target);
        });
}


ProxyLogEvents::~ProxyLogEvents() noexcept
{
    log->LogVerb1("Log proxy " + GetPath()
                  + ", receiver " + receiver_target + " removed");
}


const bool ProxyLogEvents::Authorize(const DBus::Authz::Request::Ptr req)
{
    // Only the user ID of the log proxy target and session manager should
    // be granted access to this object.  The log service will have direct
    // access to this object outside the D-Bus, so that is not accounted
    // for here.

    // Check the UID of the caller
    if (credsqry->GetUID(req->caller) == target_uid)
    {
        return true;
    }

    // Check if the busname matches unique bus name of the session manager
    const std::string sessmgr_bn = credsqry->GetUniqueBusName(
        Constants::GenServiceName("sessions"));
    if (sessmgr_bn == req->caller)
    {
        return true;
    }

    // Nothing matched - reject
    return false;
}


const std::string ProxyLogEvents::AuthorizationRejected(const DBus::Authz::Request::Ptr req) const noexcept
{
    return "Access denied";
}


std::string ProxyLogEvents::GetReceiverTarget() const noexcept
{
    return receiver_target;
}


void ProxyLogEvents::SendLog(const Events::Log &logev) const
{
    if (filter->Allow(logev))
    {
        signal_proxy->SendLog(logev);
    }
}


void ProxyLogEvents::SendStatusChange(const DBus::Object::Path &path,
                                      const Events::Status &stchgev) const
{
    signal_proxy->SendStatusChange(stchgev);
}


} // namespace LogService
