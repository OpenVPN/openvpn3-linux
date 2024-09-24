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
 * @file sessionmgr-session.cpp
 *
 * @brief Implementation of the Session Manager session object
 */


#include <gdbuspp/connection.hpp>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/glib2/utils.hpp>

#include "build-config.h"
#include "common/lookup.hpp"
#include "dbus/constants.hpp"
#include "dbus/path.hpp"
#include "log/logwriter.hpp"
#include "sessionmgr-session.hpp"


using namespace DBus;

namespace SessionManager {

class ReadyException : public DBus::Object::Method::Exception
{
  public:
    ReadyException(const std::string &msg,
                   const std::string &err_domain = "net.openvpn.v3.error.ready",
                   GError *gliberr = nullptr)
        : DBus::Object::Method::Exception(extract_error(msg), gliberr)
    {
        DBus::Exception::error_domain = std::move(err_domain);
    }

  private:
    const std::string extract_error(const std::string &input)
    {
        std::istringstream err_s;
        err_s.str(input);
        uint8_t skip = 2; // Skip the GDBus++ error prefixes
        std::ostringstream res;
        for (std::string line; std::getline(err_s, line, ':');)
        {
            if (skip > 0)
            {
                --skip;
                continue;
            }
            line.erase(0, line.find_first_not_of(" "));
            res << (res.tellp() > 0 ? ":" : "") << line;
        }
        return res.str();
    }
};


Session::Session(DBus::Connection::Ptr dbuscon,
                 DBus::Object::Manager::Ptr objmgr,
                 DBus::Credentials::Query::Ptr creds_qry_,
                 ::Signals::SessionManagerEvent::Ptr sig_sessionmgr,
                 const DBus::Object::Path &sespath,
                 const uid_t owner,
                 const std::string &be_busname,
                 const pid_t be_pid,
                 const DBus::Object::Path &cfg_path,
                 const unsigned int loglev,
                 LogWriter::Ptr logwr)
    : DBus::Object::Base(std::move(sespath), Constants::GenInterface("sessions")),
      dbus_conn(dbuscon), object_mgr(objmgr), creds_qry(creds_qry_),
      sig_sessmgr(sig_sessionmgr),
      backend_pid(be_pid), config_path(std::move(cfg_path))
{
    // Set up the D-Bus proxy towards the back-end VPN client process
    be_prx = DBus::Proxy::Client::Create(dbus_conn, be_busname);
    be_target = DBus::Proxy::TargetPreset::Create(Constants::GenPath("backends/session"),
                                                  Constants::GenInterface("backends"));

    // Prepare the signal subscription handler, this will handle
    sigsubscr = DBus::Signals::SubscriptionManager::Create(dbus_conn);

    // The subscriptions being configured is targeting the backend VPN client
    auto subscr_tgt = DBus::Signals::Target::Create(be_busname,
                                                    be_target->object_path,
                                                    be_target->interface);

    // Prepare signals the session object will send
    sig_session = Log::Create(dbus_conn, LogGroup::SESSIONMGR, GetPath(), logwr);
    sig_statuschg = sig_session->CreateSignal<::Signals::StatusChange>(sigsubscr, subscr_tgt);

    // Prepare a signal group which will do broadcasts, this will be used
    // for when proxying the AttentionRequired signals
    sig_session->GroupCreate("broadcast");
    sig_session->GroupAddTarget("broadcast", "");
    sig_attreq = sig_session->GroupCreateSignal<::Signals::AttentionRequired>("broadcast",
                                                                              sigsubscr,
                                                                              subscr_tgt);
    // Prepare the object handling access control lists
    object_acl = GDBusPP::Object::Extension::ACL::Create(dbus_conn, owner);

    //
    // Set up the D-Bus methods
    //

    AddMethod(
        "Ready",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_ready(args);
        });


    AddMethod(
        "Connect",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_connect(args);
        });

    AddMethod(
        "Restart",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_proxy_be(args, "Restart", true);
            sig_session->LogVerb2("Session restarting - " + GetPath());
        });

    auto arg_pause = AddMethod(
        "Pause",
        [=](Object::Method::Arguments::Ptr args)
        {
            GVariant *params = args->GetMethodParameters();
            auto reason = glib2::Value::Extract<std::string>(params, 0);
            method_proxy_be(args, "Pause", true);
            sig_session->LogVerb2("Session pausing " + GetPath()
                                  + ": " + reason);
        });
    arg_pause->AddInput("reason", glib2::DataType::DBus<std::string>());


    AddMethod(
        "Resume",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_proxy_be(args, "Resume", true);
            sig_session->LogVerb2("Session resuming - " + GetPath());
        });

    AddMethod(
        "Disconnect",
        [=](Object::Method::Arguments::Ptr args)
        {
            close_session(false);
            sig_session->LogVerb2("Session disconnecting - " + GetPath());
            args->SetMethodReturn(nullptr);
        });


    auto arg_grant = AddMethod(
        "AccessGrant",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_access_grant(args);
        });

    arg_grant->AddInput("uid", glib2::DataType::DBus<uint32_t>());

    auto arg_revoke = AddMethod(
        "AccessRevoke",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_access_revoke(args);
        });
    arg_revoke->AddInput("uid", glib2::DataType::DBus<uint32_t>());

    auto arg_logfwd = AddMethod(
        "LogForward",
        [=](Object::Method::Arguments::Ptr args)
        {
            method_log_forward(args);
        });
    arg_logfwd->AddInput("enable", glib2::DataType::DBus<bool>());

    auto arg_usrinpq_gettypegr = AddMethod(
        "UserInputQueueGetTypeGroup",
        [=](Object::Method::Arguments::Ptr args)
        {
            validate_vpn_backend();
            GVariant *r = be_prx->Call(be_target,
                                       "UserInputQueueGetTypeGroup");
            args->SetMethodReturn(r);
        });
    arg_usrinpq_gettypegr->AddOutput("type_group_list", "a(uu)");

    auto arg_usrinpq_fetch = AddMethod(
        "UserInputQueueFetch",
        [=](Object::Method::Arguments::Ptr args)
        {
            validate_vpn_backend();
            GVariant *r = be_prx->Call(be_target,
                                       "UserInputQueueFetch",
                                       args->GetMethodParameters());
            args->SetMethodReturn(r);
        });
    arg_usrinpq_fetch->AddInput("type", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddInput("group", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddInput("id", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddOutput("type", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddOutput("group", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddOutput("id", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_fetch->AddOutput("name", glib2::DataType::DBus<std::string>());
    arg_usrinpq_fetch->AddOutput("description", glib2::DataType::DBus<std::string>());
    arg_usrinpq_fetch->AddOutput("hidden_input", glib2::DataType::DBus<bool>());

    auto arg_usrinpq_check = AddMethod(
        "UserInputQueueCheck",
        [=](Object::Method::Arguments::Ptr args)
        {
            validate_vpn_backend();
            GVariant *r = be_prx->Call(be_target,
                                       "UserInputQueueCheck",
                                       args->GetMethodParameters());
            args->SetMethodReturn(r);
        });
    arg_usrinpq_check->AddInput("type", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_check->AddInput("group", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_check->AddOutput("indexes", "au");

    auto arg_usrinpq_provide = AddMethod(
        "UserInputProvide",
        [=](Object::Method::Arguments::Ptr args)
        {
            validate_vpn_backend();
            GVariant *r = be_prx->Call(be_target,
                                       "UserInputProvide",
                                       args->GetMethodParameters());
            args->SetMethodReturn(r);
        });
    arg_usrinpq_provide->AddInput("type", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_provide->AddInput("group", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_provide->AddInput("id", glib2::DataType::DBus<uint32_t>());
    arg_usrinpq_provide->AddInput("value", glib2::DataType::DBus<std::string>());

    // Prepare the object properties closely tied to variables in this object
    AddProperty("session_created", created, false, glib2::DataType::DBus<uint64_t>());
    AddProperty("config_path", config_path, false);
    AddProperty("config_name", config_name, false);
    AddProperty("backend_pid", backend_pid, false, glib2::DataType::DBus<uint32_t>());
    AddProperty("restrict_log_access", restrict_log_access, true);

    // Prepare object properties extracting information from other sources
    AddPropertyBySpec(
        "log_forwards",
        "ao",
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            // Avoid parallel access to the log_forwarders map;
            // see also helper_stop_log_forwards()
            std::lock_guard<std::mutex> guard(log_forwarders_mtx);

            std::vector<DBus::Object::Path> res{};
            for (const auto &[target, fwd] : log_forwarders)
            {
                res.push_back(fwd->GetPath());
            }
            return glib2::Value::CreateVector(res);
        });

    // owner: The UID of the user starting this session
    AddPropertyBySpec(
        "owner",
        glib2::DataType::DBus<uint32_t>(),
        [=](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(object_acl->GetOwner());
        });

    // acl: list of users with elevated privileges to interact
    //      with this VPN session
    AddPropertyBySpec(
        "acl",
        "au",
        [=](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::CreateVector(object_acl->GetAccessList());
        });

    // status: Current VPN session status
    AddPropertyBySpec(
        "status",
        sig_statuschg->GetSignature(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            return sig_statuschg->LastStatusChange();
        });

    // last log: The last log line seen
    AddPropertyBySpec(
        "last_log",
        "a{sv}",
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            return sig_session->GetLastLogEvent().GetGVariantDict();
        });

    // statistics: VPN interface statistics, from an OpenVPN perspective
    AddPropertyBySpec(
        "statistics",
        "a{sx}",
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            validate_vpn_backend("statistics");
            return be_prx->GetPropertyGVariant(be_target, "statistics");
        });

    // device path: D-Bus path to the interface in net.openvpn.v3.netcfg
    AddPropertyBySpec(
        "device_path",
        // Ideally, the data type here should be DBus::Object::Path,
        // but it must support an empty value until the device is configured
        glib2::DataType::DBus<std::string>(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            validate_vpn_backend("device_path");
            return be_prx->GetPropertyGVariant(be_target, "device_path");
        });

    // device name: string with the name of the virtual network interface
    AddPropertyBySpec(
        "device_name",
        glib2::DataType::DBus<std::string>(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            validate_vpn_backend("device_name");
            return be_prx->GetPropertyGVariant(be_target, "device_name");
        });

    AddPropertyBySpec(
        "session_name",
        glib2::DataType::DBus<std::string>(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            validate_vpn_backend("session_name");
            return be_prx->GetPropertyGVariant(be_target, "session_name");
        });


    //
    // Prepare object properties which has information in other object and
    // which the caller may modify
    //

    // Log level/verbosity
    AddPropertyBySpec(
        "log_verbosity",
        glib2::DataType::DBus<uint32_t>(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            return glib2::Value::Create(sig_session->GetLogLevel());
        },
        [=](const DBus::Object::Property::BySpec &prop, GVariant *value)
            -> DBus::Object::Property::Update::Ptr
        {
            auto v = glib2::Value::Get<uint32_t>(value);
            sig_session->SetLogLevel(v);
            auto upd = prop.PrepareUpdate();
            upd->AddValue(v);
            sig_session->LogInfo("Log level changed to " + std::to_string(sig_session->GetLogLevel())
                                 + " on " + GetPath());
            return upd;
        });

    // ACL - public access: Anyone can interact with this session
    AddPropertyBySpec(
        "public_access",
        glib2::DataType::DBus<bool>(),
        [=](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            return glib2::Value::Create(object_acl->GetPublicAccess());
        },
        [=](const DBus::Object::Property::BySpec &prop, GVariant *value)
            -> DBus::Object::Property::Update::Ptr
        {
            object_acl->SetPublicAccess(glib2::Value::Get<bool>(value));
            auto upd = prop.PrepareUpdate();
            upd->AddValue(object_acl->GetPublicAccess());
            std::string valstr = (object_acl->GetPublicAccess()
                                      ? "enabled"
                                      : "disabled");
            sig_session->LogInfo("Public access " + valstr + " on " + GetPath());
            return upd;
        });


    // Data Channel Offload (DCO) flag
    // This flag cannot be modified once the connection has started
    AddPropertyBySpec(
        "dco",
        glib2::DataType::DBus<bool>(),
        [&](const DBus::Object::Property::BySpec &prop)
            -> GVariant *
        {
            // If we don't have a backend session available, we use the
            // DCO setting stored this session object.
            //
            // If the status has been modified, we also want to
            // use the local setting
            //
            // If we have a backend available and the status is LOCKED
            // (in other words, not UNCHANGED nor MODIFIED), that means
            // we have to retrieve the backend status to get the
            // correct setting; the setting the connection really uses.
            //
            if (be_prx && be_target && DCOstatus::MODIFIED != dco_status)
            {
                dco = be_prx->GetProperty<bool>(be_target, "dco");
            }
            return glib2::Value::Create(dco);
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value)
            -> DBus::Object::Property::Update::Ptr
        {
            if (DCOstatus::LOCKED == dco_status)
            {
                throw DBus::Object::Property::Exception(this,
                                                        "dco",
                                                        "DCO mode cannot be changed now");
            }
            dco = glib2::Value::Get<bool>(value);
            dco_status = DCOstatus::MODIFIED;
            auto upd = prop.PrepareUpdate();
            upd->AddValue(dco);
            sig_session->LogVerb2(std::string("DCO changed to ") + (dco ? "enabled" : "disabled"));
            return upd;
        });
    RegisterSignals(sig_session);
}


Session::~Session() noexcept
{
    try
    {
        sig_sessmgr->Send(GetPath(),
                          EventType::SESS_DESTROYED,
                          object_acl->GetOwner());
    }
    catch (const DBus::Signals::Exception &excp)
    {
        std::cerr << "EXCEPTION [" << __func__ << "]: "
                  << excp.what() << std::endl;
    }
}


const bool Session::Authorize(DBus::Authz::Request::Ptr authzreq)
{
    // Early sanity check to see if the backend VPN process is accesible or not
    if (be_prx && be_target)
    {
        // Check if the backend is still alive
        try
        {
            GVariant *r = be_prx->Call(be_target, "Ping", nullptr);
            glib2::Utils::checkParams(__func__, r, "(b)");
            if (!glib2::Value::Extract<bool>(r, 0))
            {
                sig_session->LogCritical("Backend VPN did not respond correctly "
                                         "- "
                                         + GetPath());
                close_session(true);
                throw DBus::Object::Method::Exception("Backend VPN process did not respond");
            }
            g_variant_unref(r);
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            sig_session->LogCritical("Backend VPN did not respond: "
                                     + std::string(excp.GetRawError())
                                     + " - " + GetPath());
            close_session(true);
            throw DBus::Object::Method::Exception("Backend VPN process did not respond");
        }
    }
    else
    {
        // If there are no backends available, cleanup the session
        close_session(true);
        return false;
    }

    switch (authzreq->operation)
    {
    case DBus::Object::Operation::METHOD_CALL:
        {
            // These methods are always restricted to the owner only;
            // we don't provide sharing user admin rights to sessions
            for (const auto &method : {"net.openvpn.v3.sessions.AccessGrant",
                                       "net.openvpn.v3.sessions.AccessRevoke"})

            {
                if (method == authzreq->target)
                {
                    return object_acl->CheckOwnerAccess(authzreq->caller);
                }
            }
            if ("net.openvpn.v3.sessions.LogForward" == authzreq->target
                && restrict_log_access)
            {
                return object_acl->CheckOwnerAccess(authzreq->caller);
            }

            return object_acl->CheckACL(authzreq->caller, {object_acl->GetOwner()});
        }

    case DBus::Object::Operation::PROPERTY_GET:
        // Grant owner, granted users, root and the openvpn user access
        // to all properties
        return object_acl->CheckACL(authzreq->caller,
                                    {0,
                                     object_acl->GetOwner(),
                                     lookup_uid(OPENVPN_USERNAME)},
                                    true);

    case DBus::Object::Operation::PROPERTY_SET:
        {
            // The log verbosity can only be chanced by the session owner
            // unless the session grants users via the ACL access to log
            // events (!restrict_log_access)
            if (("net.openvpn.v3.log_verbosity" == authzreq->target)
                && restrict_log_access)
            {
                return object_acl->CheckOwnerAccess(authzreq->caller);
            }
            else if ("net.openvpn.v3.sessions.restrict_log_access" == authzreq->target)
            {
                return object_acl->CheckOwnerAccess(authzreq->caller);
            }
            else if ("net.openvpn.v3.sessions.public_access" == authzreq->target)
            {
                return object_acl->CheckOwnerAccess(authzreq->caller);
            }
            return object_acl->CheckACL(authzreq->caller,
                                        {object_acl->GetOwner()},
                                        true);
        }

    case DBus::Object::Operation::NONE:
    default:
        break;
    }

    return false;
}

const std::string Session::AuthorizationRejected(const Authz::Request::Ptr azreq) const noexcept
{
    if (!be_prx || !be_target)
    {
        return "Access denied: Session no longer valid";
    }

    try
    {
        if (DBus::Object::Operation::PROPERTY_GET != azreq->operation)
        {
            // Don't log rejected property gets; they are not much useful
            // and just adds lots of log noise
            std::ostringstream errmsg;
            errmsg << "Access denied to "
                   << (DBus::Object::Operation::METHOD_CALL == azreq->operation
                           ? "method"
                           : "property")
                   << " " << azreq->target << " on " << azreq->object_path
                   << " (caller: " << azreq->caller << ", user: "
                   << lookup_username(creds_qry->GetUID(azreq->caller))
                   << ", caller pid: "
                   << creds_qry->GetPID(azreq->caller) << ")";
            sig_session->LogCritical(errmsg.str());
        }
    }
    catch (...)
    {
        std::cerr << "EXCEPTION: "
                  << "Access denied:" << azreq << std::endl;
    }
    return "Access denied";
}


void Session::SetConfigName(const std::string &cfgname)
{
    if (config_name.empty())
    {
        config_name = cfgname;
    }
}


const std::string Session::GetConfigName() const noexcept
{
    return config_name;
}


const std::string Session::GetDeviceName() const noexcept
{
    validate_vpn_backend("device_name");
    return be_prx->GetProperty<std::string>(be_target, "device_name");
}


const bool Session::CheckACL(const std::string &caller) const noexcept
{
    return object_acl->CheckACL(caller, {object_acl->GetOwner()});
}


const uid_t Session::GetOwner() const noexcept
{
    return object_acl->GetOwner();
}


void Session::MoveToOwner(const uid_t from_uid, const uid_t to_uid)
{
    object_acl->TransferOwnership(to_uid);
    object_acl->GrantAccess(from_uid);
    restrict_log_access = false; // The previous owner can receive log events
    sig_session->LogInfo("Ownership changed from " + lookup_username(from_uid)
                         + " to " + lookup_username(to_uid)
                         + " on " + GetPath());
}


void Session::method_ready(DBus::Object::Method::Arguments::Ptr args)
{
    validate_vpn_backend();

    try
    {
        GVariant *r = be_prx->Call(be_target, "Ready", nullptr);
        g_variant_unref(r);
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        std::string err(excp.what());

        if (err.find("net.openvpn.v3.error.ready"))
        {

            throw ReadyException(excp.what());
        }
        sig_session->LogCritical(excp.GetRawError());
        throw DBus::Object::Method::Exception(excp.GetRawError());
    }
    args->SetMethodReturn(nullptr);
}


void Session::method_proxy_be(DBus::Object::Method::Arguments::Ptr args,
                              const std::string &method,
                              const bool no_response)
{
    validate_vpn_backend();

    GVariant *r = be_prx->Call(be_target,
                               method,
                               args->GetMethodParameters());
    if (no_response)
    {
        g_variant_unref(r);
        r = nullptr;
    };
    args->SetMethodReturn(r);
}


void Session::method_connect(DBus::Object::Method::Arguments::Ptr args)
{
    validate_vpn_backend();

    // Set and lock the DCO mode
    if (DCOstatus::MODIFIED == dco_status)
    {
        sig_session->Debug(std::string("DCO setting changed to ")
                           + (dco ? "enabled" : "disabled"));
        be_prx->SetProperty(be_target, "dco", dco);
    }
    dco_status = DCOstatus::LOCKED;

    auto loglvl = be_prx->GetProperty<uint32_t>(be_target, "log_level");
    sig_session->SetLogLevel(loglvl);

    GVariant *r = be_prx->Call(be_target, "Connect", nullptr);
    g_variant_unref(r);
    sig_session->LogVerb2("Starting connection - " + GetPath());
    args->SetMethodReturn(nullptr);
}


void Session::method_log_forward(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    bool enable = glib2::Value::Extract<bool>(params, 0);

    std::string caller = args->GetCallerBusName();

    // Avoid parallel access to the log_forwarders map;
    // see also helper_stop_log_forwards()
    std::lock_guard<std::mutex> guard(log_forwarders_mtx);

    if (enable)
    {
        auto logservice = LogServiceProxy::Create(dbus_conn);
        log_forwarders[caller] = logservice->ProxyLogEvents(caller, GetPath());
        sig_session->LogVerb2("Added log forwarding to " + caller
                              + " on " + GetPath()
                              + " (user: "
                              + lookup_username(creds_qry->GetUID(caller))
                              + ")");
    }
    else
    {
        try
        {
            log_forwarders.at(caller)->Remove();
            log_forwarders.at(caller).reset();
            log_forwarders.erase(caller);
        }
        catch (const std::exception &e)
        {
            sig_session->LogCritical("log_forwarders.erase(" + caller + ") "
                                     + "failed:" + std::string(e.what()));
        }
        sig_session->LogVerb2("Removed log forwarding from " + caller
                              + " on " + GetPath()
                              + " (user: "
                              + lookup_username(creds_qry->GetUID(caller))
                              + ")");
    }
    args->SetMethodReturn(nullptr);
}


void Session::method_access_grant(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto user = glib2::Value::Extract<uid_t>(params, 0);
    try
    {
        object_acl->GrantAccess(user);
    }
    catch (const GDBusPP::Object::Extension::ACLException &excp)
    {
        throw DBus::Object::Method::Exception(excp.GetRawError());
    }
    sig_session->LogInfo("Granted access to " + lookup_username(user)
                         + " on " + GetPath());
}


void Session::method_access_revoke(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    auto user = glib2::Value::Extract<uid_t>(params, 0);
    try
    {
        object_acl->RevokeAccess(user);
    }
    catch (const GDBusPP::Object::Extension::ACLException &excp)
    {
        throw DBus::Object::Method::Exception(excp.GetRawError());
    }
    sig_session->LogInfo("Revoked access from " + lookup_username(user)
                         + " on " + GetPath());
}


void Session::close_session(const bool forced)
{
    try
    {
        validate_vpn_backend();
        GVariant *r = be_prx->Call(be_target,
                                   (!forced ? "Disconnect" : "ForceShutdown"));
        if (r)
        {
            g_variant_unref(r);
        }
    }
    catch (const DBus::Exception &)
    {
        // Ignore errors here; backend client may be dead already
        be_prx.reset();
        be_target.reset();
    }

    if (!forced)
    {
        sig_statuschg->Send(Events::Status(StatusMajor::SESSION,
                                           StatusMinor::PROC_STOPPED,
                                           "Session closed"));
    }
    else
    {
        sig_statuschg->Send(Events::Status(StatusMajor::SESSION,
                                           StatusMinor::PROC_KILLED,
                                           "Session closed, killed backend client"));
    }

    helper_stop_log_forwards();
    sig_session->LogVerb1("Session closing - " + GetPath());
    try
    {
        object_mgr->RemoveObject(GetPath());
    }
    catch (const DBus::Exception &excp)
    {
        sig_session->Debug("close_session(): " + std::string(excp.what()));
    }
}


void Session::helper_stop_log_forwards()
{
    // Avoid parallel runs of this method; it depends
    // on a stable 'log_forwarders'
    std::lock_guard<std::mutex> guard(log_forwarders_mtx);

    for (auto &[owner_busid, obj] : log_forwarders)
    {
        try
        {
            obj->Remove();
        }
        catch (const std::exception &e)
        {
            sig_session->LogCritical("log_forwarders.erase(" + owner_busid + ") "
                                     + "failed:" + std::string(e.what()));
        }
        sig_session->LogVerb2("Session closing, Removed log forwarding from "
                              + owner_busid
                              + " on " + GetPath()
                              + " (user: "
                              + lookup_username(creds_qry->GetUID(owner_busid))
                              + ")");
    }
    log_forwarders.clear();
}


void Session::validate_vpn_backend(const std::string &property) const
{
    if (!be_prx || !be_target)
    {
        if (property.empty())
        {
            // Without a property name, this is used in context of a
            // D-Bus method call - throw a method exception
            throw DBus::Object::Method::Exception("VPN session is stopped");
        }
        else
        {
            // A property name was provided, throw a property exception
            throw DBus::Object::Property::Exception(this,
                                                    property,
                                                    "VPN session is stopped");
        }
    }
}

} // namespace SessionManager
