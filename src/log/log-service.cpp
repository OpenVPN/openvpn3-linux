//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file log-service.cpp
 *
 *  @brief Implements the basic net.openvpn.v3.log service handler
 */

#include <string>

#include "common/lookup.hpp"
#include "log-internal.hpp"
#include "log-service.hpp"
#include "log-proxylog.hpp"

namespace LogService {

class MethodError : public DBus::Object::Method::Exception
{
  public:
    MethodError(const std::string &msg)
        : DBus::Object::Method::Exception(msg)
    {
        error_domain = Constants::GenServiceName("log.error");
    }
};



//
//
//  LogService::AttachedService
//
//



AttachedService::Ptr AttachedService::Create(
    DBus::Connection::Ptr conn,
    DBus::Object::Manager::Ptr object_mgr,
    LogService::Logger::Ptr log,
    DBus::Signals::SubscriptionManager::Ptr submgr,
    LogTag::Ptr tag,
    const std::string &busname,
    const std::string &interface)
{
    return Ptr(new AttachedService(conn,
                                   object_mgr,
                                   log,
                                   submgr,
                                   tag,
                                   busname,
                                   interface));
}


AttachedService::AttachedService(DBus::Connection::Ptr conn,
                                 DBus::Object::Manager::Ptr obj_mgr,
                                 LogService::Logger::Ptr logr,
                                 DBus::Signals::SubscriptionManager::Ptr submgr,
                                 LogTag::Ptr tag,
                                 const std::string &busname,
                                 const std::string &interface)
    : logtag(tag),
      src_target(DBus::Signals::Target::Create(busname, "", interface)),
      connection(conn), object_mgr(obj_mgr), log(logr)
{
    log_handler = Signals::ReceiveLog::Create(
        submgr,
        src_target,
        [&](Events::Log logevent)
        {
            process_log_event(logevent);
        });

    status_handler = Signals::ReceiveStatusChange::Create(
        submgr,
        src_target,
        [&](const std::string &sender,
            const DBus::Object::Path &path,
            const std::string &interface,
            Events::Status statusev)
        {
            process_statuschg_event(sender, path, interface, statusev);
        });
}


AttachedService::~AttachedService() noexcept
{
    // Remove all the ProxyLogEevents child objects explictly
    log->Debug("Removing AttachedService object for " + logtag->str());
    for (auto &[key, obj] : proxies)
    {
        log->Debug("Service cleanup: Disconnecting log proxy on " + key);
        obj.reset();
    }
}


DBus::Object::Path AttachedService::AddProxyTarget(const std::string &recv_tgt,
                                                   const DBus::Object::Path &session_path)
{
    auto proxy_obj = object_mgr->CreateObject<ProxyLogEvents>(
        connection,
        object_mgr,
        recv_tgt,
        session_path,
        src_target->object_interface,
        6);

    object_mgr->AttachRemoveCallback(
        proxy_obj->GetPath(),
        [&](const DBus::Object::Path &path)
        {
            auto tgt = proxies.at(path)->GetReceiverTarget();
            proxies.erase(path);
            log->LogVerb1("Log proxy " + path
                          + ", receiver " + tgt + " removed");
        });

    proxies[proxy_obj->GetPath()] = proxy_obj;
    log->LogVerb1("Log proxy configured for " + session_path
                  + " on " + proxy_obj->GetPath()
                  + " sending to " + recv_tgt);
    return proxy_obj->GetPath();
}


void AttachedService::OverrideObjectPath(const DBus::Object::Path &new_path)
{
    override_obj_path = new_path;
}


void AttachedService::process_log_event(const Events::Log &logevent)
{
    auto meta = LogMetaData::Create();
    meta->AddMeta("sender", logevent.sender->busname);
    if (!override_obj_path.empty())
    {
        meta->AddMeta("object_path", override_obj_path);
        meta->AddMeta("sender_object_path", logevent.sender->object_path);
    }
    else
    {
        meta->AddMeta("object_path", logevent.sender->object_path);
    }
    meta->AddMeta("interface", logevent.sender->object_interface);

    Events::Log local_event(logevent);
    local_event.AddLogTag(logtag);
    log->Log(local_event, meta);

    for (const auto &[proxy_tgt, sig_proxy] : proxies)
    {
        Events::Log ev(logevent);
        ev.RemoveToken();
        sig_proxy->SendLog(ev);
    }
}


void AttachedService::process_statuschg_event(const std::string &sender,
                                              const DBus::Object::Path &path,
                                              const std::string &interface,
                                              const Events::Status &statuschg)
{
    for (const auto &[proxy_tgt, sig_proxy] : proxies)
    {
        sig_proxy->SendStatusChange(path, statuschg);
    }
}

//
//
//  LogService::ServiceHandler
//
//



ServiceHandler::ServiceHandler(DBus::Connection::Ptr connection_,
                               DBus::Object::Manager::Ptr obj_mgr,
                               Configuration &&cfgobj)
    : DBus::Object::Base(Constants::GenPath("log"),
                         Constants::GenInterface("log")),
      connection(connection_), object_mgr(obj_mgr), config(cfgobj),
      logwr(cfgobj.servicelog->GetLogWriter()),
      log(cfgobj.servicelog),
      logfilter(cfgobj.logfilter)
{
    DisableIdleDetector(true);
    dbuscreds = DBus::Credentials::Query::Create(connection);
    subscrmgr = DBus::Signals::SubscriptionManager::Create(connection);

    auto meth_attach = AddMethod(
        "Attach",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            method_attach(args);
        });
    meth_attach->AddInput("interface",
                          glib2::DataType::DBus<std::string>());


    auto meth_assign = AddMethod(
        "AssignSession",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            method_assign_session(args);
        });
    meth_assign->AddInput("session_path",
                          glib2::DataType::DBus<DBus::Object::Path>());
    meth_assign->AddInput("interface",
                          glib2::DataType::DBus<std::string>());


    auto meth_detach = AddMethod(
        "Detach",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            method_detach(args);
        });
    meth_detach->AddInput("interface",
                          glib2::DataType::DBus<std::string>());


    auto meth_getsublst = AddMethod(
        "GetSubscriberList",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            method_get_subscr_list(args);
        });
    meth_getsublst->AddOutput("subscribers", "a(ssss)");


    auto meth_proxylogev = AddMethod(
        "ProxyLogEvents",
        [&](DBus::Object::Method::Arguments::Ptr args)
        {
            method_proxy_log_events(args);
        });
    meth_proxylogev->AddInput("target_address",
                              glib2::DataType::DBus<std::string>());
    meth_proxylogev->AddInput("session_path",
                              glib2::DataType::DBus<DBus::Object::Path>());
    meth_proxylogev->AddOutput("proxy_path",
                               glib2::DataType::DBus<DBus::Object::Path>());

    AddProperty("version", version, false);
    AddProperty("log_method", config.log_method, false);

    AddPropertyBySpec(
        "log_dbus_details",
        glib2::DataType::DBus<bool>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(logwr->LogMetaEnabled());
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            logwr->EnableLogMeta(glib2::Value::Get<bool>(value));
            return save_property(prop, "service-log-dbus-details", logwr->LogMetaEnabled());
        });

    AddPropertyBySpec(
        "log_prefix_logtag",
        glib2::DataType::DBus<bool>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(logwr->MessagePrependEnabled());
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            logwr->EnableMessagePrepend(glib2::Value::Get<bool>(value));
            return save_property(prop, "no-logtag-prefix", !logwr->MessagePrependEnabled());
        });

    AddPropertyBySpec(
        "timestamp",
        glib2::DataType::DBus<bool>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(logwr->TimestampEnabled());
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            logwr->EnableTimestamp(glib2::Value::Get<bool>(value));
            return save_property(prop, "timestamp", logwr->TimestampEnabled());
        });


    AddPropertyBySpec(
        "log_level",
        glib2::DataType::DBus<uint32_t>(),
        [&](const DBus::Object::Property::BySpec &prop)
        {
            return glib2::Value::Create(logfilter->GetLogLevel());
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            logfilter->SetLogLevel(glib2::Value::Get<uint32_t>(value));
            return save_property(prop, "log-level", static_cast<int>(logfilter->GetLogLevel()));
        });

    AddPropertyBySpec(
        "config_file",
        glib2::DataType::DBus<std::string>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            std::string fn = "";
            if (config.config_file)
            {
                fn = config.config_file->GetFilename();
            }
            return glib2::Value::Create(fn);
        });

    AddPropertyBySpec(
        "num_attached",
        glib2::DataType::DBus<uint32_t>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create<uint32_t>(log_attach_subscr.size());
        });
}


const bool ServiceHandler::Authorize(const DBus::Authz::Request::Ptr req)
{
    if (DBus::Object::Operation::METHOD_CALL == req->operation)
    {
        if ("net.openvpn.v3.log.AssignSession" == req->target)
        {
            // All VPN backend client processes (openvpn3-service-client) have
            // a well-known D-Bus name being net.openvpn.v3.backends.be$PID,
            // where $PID is the process ID of the proccess.  This process
            // owner should also be the OPENVPN_USERNAME.
            //
            // This checks that the caller is from this process and has the
            // proper credentials
            return check_busname_vpn_client(req->caller);
        }
        else if ("net.openvpn.v3.log.ProxyLogEvents" == req->target)
        {
            // This is only available to the net.openvpn.v3.session service
            // when accessed as the OPENVPN_USERNAME
            return check_busname_service_name(req->caller,
                                              Constants::GenServiceName("sessions"));
        }
    }
    return true;
};


// LogService::ServiceHandler - D-Bus method callback functions


void ServiceHandler::method_attach(DBus::Object::Method::Arguments::Ptr args)
{
    cleanup_service_subscriptions();

    LogMetaData::Ptr meta = internal::prepare_metadata(this, "Attach", args);
    pid_t caller_pid = -1;
    try
    {
        caller_pid = dbuscreds->GetPID(args->GetCallerBusName());
    }
    catch (const DBus::Credentials::Exception &excp)
    {
        log->LogCritical("Attach error: " + std::string(excp.what()), meta);
        throw MethodError("Could not authenticate caller");
    }

    GVariant *params = args->GetMethodParameters();
    auto interface = glib2::Value::Extract<std::string>(params, 0);
    auto tag = LogTag::Create(args->GetCallerBusName(), interface);

    if (log_attach_subscr.find(tag->hash) != log_attach_subscr.end())
    {
        std::ostringstream msg;
        msg << "Duplicated attach request: " << *tag
            << ", pid: " << std::to_string(caller_pid);
        log->LogWarn(msg.str());
        throw MethodError("Already attached");
    }

    log_attach_subscr[tag->hash] = AttachedService::Create(connection,
                                                           object_mgr,
                                                           log,
                                                           subscrmgr,
                                                           tag,
                                                           args->GetCallerBusName(),
                                                           interface);
    std::ostringstream msg;
    msg << "Attached: " << *tag << "  " << tag->tag
        << ", pid " << std::to_string(caller_pid);
    log->LogVerb2(msg.str(), meta);
    args->SetMethodReturn(nullptr);

    // Since a service is now attached, the idle detector needs to consider
    // this object's presence when checking if the service is idle or not.
    // Normally this is not needed, since the service creates child objects.
    // This does not happen in this case.  The call below reverses the
    // idle detector setting done in the contructor.
    DisableIdleDetector(false);
}


void ServiceHandler::method_assign_session(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto session_path = glib2::Value::Extract<DBus::Object::Path>(params, 0);
    auto interface = glib2::Value::Extract<std::string>(params, 1);

    // TODO: The ProxyLogEvents() API is lacking the interface aspect - so for
    // now the interface cannot be part of the index key
    SessionInterfKey key{session_path, ""};
    auto tag = LogTag::Create(args->GetCallerBusName(), interface);
    session_logtag_index[key] = tag->hash;
    log_attach_subscr[tag->hash]->OverrideObjectPath(session_path);
    log->Debug("Assigned session " + session_path
               + ", interface=" + interface + " to " + tag->str());
    args->SetMethodReturn(nullptr);
}


void ServiceHandler::method_detach(DBus::Object::Method::Arguments::Ptr args)
{
    cleanup_service_subscriptions();
    LogMetaData::Ptr meta = internal::prepare_metadata(this, "Detach", args);

    pid_t caller_pid = -1;
    try
    {
        caller_pid = dbuscreds->GetPID(args->GetCallerBusName());
    }
    catch (const DBus::Credentials::Exception &excp)
    {
        // This error is quite common when the attached service exits
        // quickly after the Detach call.  It is safe ignore these errors
        // since the caller_pid is only used to provide more log details
        //
        // log->Debug("Detach error: " + std::string(excp.what()), meta);
    }

    GVariant *params = args->GetMethodParameters();
    auto interface = glib2::Value::Extract<std::string>(params, 0);
    auto tag = LogTag::Create(args->GetCallerBusName(), interface);

    if (!check_detach_access(tag, args->GetCallerBusName()))
    {
        std::ostringstream msg;
        msg << "Attempt to Detach log subscription for " << tag->str()
            << ", caller " << args->GetCallerBusName()
            << ", interface " << interface;
        log->LogWarn(msg.str());
        throw MethodError("Access denied");
    }

    if (log_attach_subscr.find(tag->hash) == log_attach_subscr.end())
    {
        std::ostringstream msg;
        msg << "Could not find an attach subscription for " << tag->tag
            << ", pid " << (caller_pid > 0 ? std::to_string(caller_pid) : "unknown")
            << ", caller " << args->GetCallerBusName();
        log->LogWarn(msg.str(), meta);
        throw MethodError("Not attached");
    }

    remove_log_subscription(tag, caller_pid, meta);
    args->SetMethodReturn(nullptr);
}


void ServiceHandler::method_get_subscr_list(DBus::Object::Method::Arguments::Ptr args)
{
    cleanup_service_subscriptions();

    auto bld = glib2::Builder::Create("a(ssss)");
    for (const auto &[tag, sub] : log_attach_subscr)
    {
        auto elmnt_bld = glib2::Builder::Create("(ssss)");
        glib2::Builder::Add(elmnt_bld, std::to_string(tag));
        glib2::Builder::Add(elmnt_bld, sub->src_target->busname);
        glib2::Builder::Add(elmnt_bld, sub->src_target->object_interface);
        glib2::Builder::Add(elmnt_bld, std::string(sub->src_target->object_path));
        glib2::Builder::Add(bld, glib2::Builder::Finish(elmnt_bld));
    }
    args->SetMethodReturn(glib2::Builder::FinishWrapped(bld));
}


void ServiceHandler::method_proxy_log_events(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto target = glib2::Value::Extract<std::string>(params, 0);
    auto session_path = glib2::Value::Extract<DBus::Object::Path>(params, 1);

    try
    {
        SessionInterfKey lookup_key{session_path, ""};
        auto tag = session_logtag_index.at(lookup_key);
        auto proxypath = log_attach_subscr.at(tag)->AddProxyTarget(target,
                                                                   session_path);
        args->SetMethodReturn(glib2::Value::CreateTupleWrapped(proxypath));
    }
    catch (const std::out_of_range &)
    {
        throw MethodError("No session available");
    }
}


// LogService::ServiceHandler - Misc private methods


const bool ServiceHandler::check_busname_vpn_client(const std::string &caller) const
{
    pid_t c_pid = dbuscreds->GetPID(caller);
    std::string chk_be_busname = Constants::GenServiceName("backends.be")
                                 + std::to_string(c_pid);
    return check_busname_service_name(caller, chk_be_busname);
}


const bool ServiceHandler::check_busname_service_name(const std::string &caller,
                                                      const std::string &srvname) const
{
    try
    {
        uid_t c_uid = dbuscreds->GetUID(caller);
        if (lookup_uid(OPENVPN_USERNAME) != c_uid)
        {
            std::ostringstream msg;
            msg << "Caller " << caller
                << " attempted accessing AssignSession"
                << " as user " << lookup_username(c_uid) << "."
                << " Rejected.";
            log->LogWarn(msg.str());
            return false;
        }

        std::string chk_uniq_busname = dbuscreds->GetUniqueBusName(srvname);
        if (caller != chk_uniq_busname)
        {
            std::ostringstream msg;
            msg << "Caller " << caller
                << " attempted accessing AssignSession"
                << " while not being a VPN backend process"
                << " Rejected.";
            log->LogWarn(msg.str());
            return false;
        }
        return true;
    }
    catch (const DBus::Credentials::Exception &)
    {
        std::ostringstream msg;
        msg << "Caller " << caller
            << " attempted accessing AssignSession"
            << " while not being a VPN backend process"
            << " Rejected.";
        log->LogWarn(msg.str());
        return false;
    }
}


void ServiceHandler::remove_log_subscription(LogTag::Ptr tag,
                                             pid_t pid,
                                             LogMetaData::Ptr meta)
{
    if (!tag)
    {
        return;
    };

    // It is needed to remove the {session path, interface} -> log tag hash
    // lookup index entry.  But the only available information is the log tag
    // hash.  One way to solve this is to do a "reverse lookup", by looking
    // up the log tag hash value instead.  That gives us the iterator entry
    // which can be used to remove the entry from the index.
    size_t tag_hash = tag->hash;
    auto sesstag_idx = std::find_if(session_logtag_index.begin(),
                                    session_logtag_index.end(),
                                    [tag_hash](const auto &element)
                                    {
                                        return element.second == tag_hash;
                                    });
    if (session_logtag_index.end() != sesstag_idx)
    {
        session_logtag_index.erase(sesstag_idx);
    }

    // Remove the AttachedService element, which will unsubscribe the
    // Log + StatusChange signals for this sender
    log_attach_subscr.at(tag->hash).reset();
    log_attach_subscr.erase(tag->hash);

    std::ostringstream msg;
    msg << "Detached: " << *tag << "  " << tag->tag;
    if (0 < pid)
    {
        msg << ", pid " << std::to_string(pid);
    }
    log->LogVerb2(msg.str(), meta);

    if (log_attach_subscr.size() < 1)
    {
        // In method_attach(), the idle detector check was re-enabled for
        // this ServiceHandler object when the Attach() method completed
        // successfully.  This was done to avoid this log service to idle
        // exit even though external services still had attached their
        // logging to this service.  When there are no more services
        // attached, this idle check logic can be re-enabled, where the
        // presence of this ServiceHandler object should no longer be
        // considered in the idle check.
        DisableIdleDetector(true);
    }
}


const bool ServiceHandler::check_detach_access(LogTag::Ptr tag,
                                               const std::string &caller) const
{
    // Lookup if there is a service attached with the given log tag
    const auto rec = log_attach_subscr.find(tag->hash);
    if (rec == log_attach_subscr.end())
    {
        // This may seem like the wrong logic.  The detail is that
        // there exist no service attach subscription from this caller
        // but that is not an AUTHORIZATION failure.  We instead let
        // this pass now, and do the proper error reporting a bit later in
        // method_detach() ... the caller won't be able to do anything
        // anyhow, so this is considered safe enough.
        return true;
    }

    // A match was found - restrict the access to the service
    // owning the LogTag
    return (caller == rec->second->src_target->busname);
}



void ServiceHandler::cleanup_service_subscriptions()
{
    std::lock_guard<std::mutex> guard(attachmap_mtx);
    std::vector<LogTag::Ptr> remove_list{};
    for (const auto &it : log_attach_subscr)
    {
        auto sub = it.second;
        try
        {
            if (sub && sub->src_target && !sub->src_target->busname.empty())
            {
                (void)dbuscreds->GetPID(sub->src_target->busname);
            }
        }
        catch (const DBus::Credentials::Exception &)
        {
            // If the PID lookup for a busname failed - there are no
            // process owning that busname any more
            if (sub)
            {
                remove_list.push_back(sub->logtag);
            }
        }
    }

    // NOTE: It's tempting to simplify this by putting the
    // remove_log_subscription() in the catch() block above - but we can't
    // modify the std::map while iterating it; that results in illegal reads.
    if (remove_list.size() > 0)
    {
        auto meta = LogMetaData::Create();
        meta->AddMeta("internal_method",
                      "LogService::ServiceHandler::cleanup_service_subscriptions");
        for (const auto &tag : remove_list)
        {
            remove_log_subscription(tag, 0, meta);
        }
    }
}



//
//
//  LogService::MainService
//
//



MainService::MainService(DBus::Connection::Ptr conn)
    : DBus::Service(conn, Constants::GenServiceName("log"))
{
}


void MainService::BusNameAcquired(const std::string &busname)
{
}


void MainService::BusNameLost(const std::string &busname)
{
    throw DBus::Service::Exception("Lost D-Bus bus name '" + busname + "'");
}


} // namespace LogService
