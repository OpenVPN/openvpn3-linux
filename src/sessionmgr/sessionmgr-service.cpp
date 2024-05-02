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
 * @file sessionmgr-service.cpp
 *
 * @brief Implementation of the net.openvpn.v3.sessions D-Bus service
 */

#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/exceptions.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/service.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "dbus/constants.hpp"
#include "dbus/path.hpp"
#include "sessionmgr-service.hpp"
#include "sessionmgr-session.hpp"
#include "sessionmgr-signals.hpp"
#include "tunnel-queue.hpp"


namespace SessionManager {


SrvHandler::SrvHandler(DBus::Connection::Ptr con,
                       DBus::Object::Manager::Ptr objmgr,
                       LogWriter::Ptr lwr)
    : DBus::Object::Base(Constants::GenPath("sessions"),
                         Constants::GenInterface("sessions")),
      dbuscon(con), object_mgr(objmgr), logwr(lwr)
{
    DisableIdleDetector(true);

    sig_sessmgr = SessionManager::Log::Create(dbuscon,
                                              LogGroup::SESSIONMGR,
                                              GetPath(),
                                              logwr);
    RegisterSignals(sig_sessmgr);

    // Prepare the SessionManagerEvent signal
    sig_sessmgr->GroupCreate("broadcast");
    sig_sessmgr->GroupAddTarget("broadcast", "");
    sig_sessmgr_event = sig_sessmgr->GroupCreateSignal<::Signals::SessionManagerEvent>("broadcast");

    creds_qry = DBus::Credentials::Query::Create(dbuscon);
    tunnel_queue = NewTunnelQueue::Create(dbuscon,
                                          creds_qry,
                                          object_mgr,
                                          logwr,
                                          sig_sessmgr,
                                          sig_sessmgr_event);


    auto new_tun = AddMethod("NewTunnel",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 this->method_new_tunnel(args);

                                 // FIXME: This is a hackish workaround to
                                 // avoid issues with the Python code not
                                 // able to yet handle an object path
                                 // which is not available yet.  This simple
                                 // sleep allows the backend client to register
                                 // itself.
                                 //
                                 // This was not needed with the previous
                                 // implementation because this call returned
                                 // after the Session object was created - and
                                 // the handling of method calls was not handled
                                 // asynchronously - which basically hid this
                                 // this issue.
                                 //
                                 // This can be removed once the Python code
                                 // is updated
                                 usleep(700000);
                             });
    new_tun->AddInput("config_path", glib2::DataType::DBus<DBus::Object::Path>());
    new_tun->AddOutput("session_path", glib2::DataType::DBus<DBus::Object::Path>());

    auto fetch_sessions = AddMethod("FetchAvailableSessions",
                                    [this](DBus::Object::Method::Arguments::Ptr args)
                                    {
                                        this->method_fetch_avail_sessions(args);
                                    });
    fetch_sessions->AddOutput("paths", "ao");

    auto fetch_mgtd_intf = AddMethod("FetchManagedInterfaces",
                                     [this](DBus::Object::Method::Arguments::Ptr args)
                                     {
                                         this->method_fetch_managed_interf(args);
                                     });
    fetch_mgtd_intf->AddOutput("devices", "as");

    auto lookup_cfgn = AddMethod("LookupConfigName",
                                 [this](DBus::Object::Method::Arguments::Ptr args)
                                 {
                                     this->method_lookup_config(args);
                                 });
    lookup_cfgn->AddInput("config_name", glib2::DataType::DBus<std::string>());
    lookup_cfgn->AddOutput("session_paths", "ao");

    auto lookup_intf = AddMethod("LookupInterface",
                                 [this](DBus::Object::Method::Arguments::Ptr args)
                                 {
                                     this->method_lookup_interf(args);
                                 });
    lookup_intf->AddInput("device_name", glib2::DataType::DBus<std::string>());
    lookup_intf->AddOutput("session_path", glib2::DataType::DBus<DBus::Object::Path>());

    AddProperty("version", version, false);

    sig_sessmgr->LogInfo("OpenVPN 3 Session Manager started");
}


void SrvHandler::SetLogLevel(const uint8_t loglvl)
{
    sig_sessmgr->SetLogLevel(loglvl);
}


const bool SrvHandler::Authorize(const Authz::Request::Ptr request)
{
    // There is no ACL management in the service handler object
    return true;
}


void SrvHandler::method_new_tunnel(Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();

    auto cfgpath = glib2::Value::Extract<DBus::Object::Path>(params, 0);
    uid_t owner = creds_qry->GetUID(args->GetCallerBusName());
    auto sespath = tunnel_queue->AddTunnel(cfgpath, owner);
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(sespath));
}


void SrvHandler::method_fetch_avail_sessions(Object::Method::Arguments::Ptr args)
{
    std::vector<DBus::Object::Path> session_paths{};

    auto no_filter = [](std::shared_ptr<Session> obj)
    {
        // we want all objects; nothing to filter out
        return true;
    };

    for (const auto &obj : helper_retrieve_sessions(args->GetCallerBusName(),
                                                    no_filter))
    {
        session_paths.push_back(obj->GetPath());
    }
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(session_paths));
}


void SrvHandler::method_fetch_managed_interf(Object::Method::Arguments::Ptr args)
{
    std::vector<std::string> devices{};

    auto search_filter = [](std::shared_ptr<Session> sessobj)
    {
        // We want to list interfaces for all available sessions; no filtering
        return true;
    };
    for (const auto &obj : helper_retrieve_sessions(args->GetCallerBusName(),
                                                    search_filter))
    {
        devices.push_back(obj->GetDeviceName());
    }
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(devices));
}


void SrvHandler::method_lookup_config(Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto search_cfgname = glib2::Value::Extract<std::string>(params, 0);

    std::vector<DBus::Object::Path> session_paths{};

    // The search filter will retrieve only sessions with specific
    // configuration names
    auto search_filter = [&search_cfgname](std::shared_ptr<Session> sessobj)
    {
        return sessobj->GetConfigName() == search_cfgname;
    };

    for (const auto &obj : helper_retrieve_sessions(args->GetCallerBusName(),
                                                    search_filter))
    {
        session_paths.push_back(obj->GetPath());
    }
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(session_paths));
}


void SrvHandler::method_lookup_interf(Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto search_ifname = glib2::Value::Extract<std::string>(params, 0);

    // The search filter will retrieve only sessions with specific
    // configuration names
    auto search_filter = [&search_ifname](std::shared_ptr<Session> sessobj)
    {
        return sessobj->GetDeviceName() == search_ifname;
    };

    auto match = helper_retrieve_sessions(args->GetCallerBusName(),
                                          search_filter);
    if (match.size() == 0)
    {
        throw DBus::Object::Method::Exception("Interface not found");
    }
    else if (match.size() > 1)
    {
        throw DBus::Object::Method::Exception("More than one interface found");
    }

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(match[0]->GetPath()));
}


SessionCollection SrvHandler::helper_retrieve_sessions(const std::string &caller,
                                                       fn_search_filter &&filter_fn) const
{
    std::vector<std::shared_ptr<Session>> sessions{};
    bool root_path_found = false;
    for (const auto &it : object_mgr->GetAllObjects())
    {
        auto path = it.first;
        if (!root_path_found && "/net/openvpn/v3/sessions" == path)
        {
            // We skip "/net/openvpn/v3/sessions" - that is not a session path
            root_path_found = true;
            continue;
        }

        auto sess_obj = std::static_pointer_cast<Session>(it.second);

        // If the caller is empty, we don't do any ACL checks
        bool caller_check = ((!caller.empty() && sess_obj->CheckACL(caller))
                             || caller.empty());

        // If the device object is not null, the path should be valid
        if (sess_obj && caller_check && filter_fn(sess_obj))
        {
            sessions.push_back(sess_obj);
        }
    }
    return sessions;
}



//
//
//  SessionManager::Service
//
//// parentheses



Service::Service(DBus::Connection::Ptr con, LogWriter::Ptr lwr)
    : DBus::Service(con, Constants::GenServiceName("sessions")),
      logwr(std::move(lwr))
{
    logsrvprx = LogServiceProxy::Create(con);
    logsrvprx->AttachInterface(con, Constants::GenInterface("sessions"));
}


Service::~Service() noexcept
{
    if (logsrvprx)
    {
        logsrvprx->Detach(Constants::GenInterface("sessions"));
    }
}


void Service::BusNameAcquired(const std::string &busname)
{
    auto srvh = CreateServiceHandler<SrvHandler>(GetConnection(),
                                                 GetObjectManager(),
                                                 logwr);
    srvh->SetLogLevel(log_level);
}


void Service::BusNameLost(const std::string &busname)
{
    throw DBus::Service::Exception("openvpn3-service-sessions lost the '"
                                   + busname + "' registration on the D-Bus");
}


void Service::SetLogLevel(const uint8_t loglvl)
{
    log_level = loglvl;
}

} // namespace SessionManager
