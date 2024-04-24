//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

/**
 *  @file tunnel-queue.hpp
 *
 */

#include "common/lookup.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr-session.hpp"
#include "tunnel-queue.hpp"


namespace SessionManager {

TunnelRecord::Ptr TunnelRecord::Create(const DBus::Object::Path &cfgpath, const uid_t owner_uid)
{
    return TunnelRecord::Ptr(new TunnelRecord(cfgpath, owner_uid));
}


TunnelRecord::TunnelRecord(const DBus::Object::Path &cfgpath, const uid_t owner_uid)
    : config_path(std::move(cfgpath)), owner(owner_uid)
{
}



NewTunnelQueue::Ptr NewTunnelQueue::Create(DBus::Connection::Ptr dbuscon,
                                           DBus::Credentials::Query::Ptr creds_qry,
                                           DBus::Object::Manager::Ptr objmgr,
                                           LogWriter::Ptr logwr,
                                           SessionManager::Log::Ptr sig_log,
                                           SessionManager::Signals::SessionManagerEvent::Ptr sesmgrev)
{
    return NewTunnelQueue::Ptr(new NewTunnelQueue(dbuscon,
                                                  creds_qry,
                                                  objmgr,
                                                  logwr,
                                                  sig_log,
                                                  sesmgrev));
}


NewTunnelQueue::NewTunnelQueue(DBus::Connection::Ptr dbuscon_,
                               DBus::Credentials::Query::Ptr creds_qry_,
                               DBus::Object::Manager::Ptr objmgr,
                               LogWriter::Ptr logwr_,
                               SessionManager::Log::Ptr sig_log,
                               SessionManager::Signals::SessionManagerEvent::Ptr sesmgrev)
    : dbuscon(dbuscon_), creds_qry(creds_qry_), object_mgr(objmgr), logwr(logwr_),
      log(sig_log), sesmgr_event(sesmgrev)
{
    be_prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(dbuscon);

    signal_subscr = DBus::Signals::SubscriptionManager::Create(dbuscon);
    subscr_target = DBus::Signals::Target::Create("",
                                                  Constants::GenPath("backends/session"),
                                                  Constants::GenInterface("backends"));
    signal_subscr->Subscribe(subscr_target,
                             "RegistrationRequest",
                             [=](DBus::Signals::Event::Ptr event)
                             {
                                 process_registration(event);
                             });
}


const DBus::Object::Path NewTunnelQueue::AddTunnel(const std::string &config_path,
                                                   const uid_t owner)
{
    try
    {
        // Create a session token and prepare a tunnel record keeping
        // the details.  The TunnelRecord will generate the session path this
        // backend VPN client process can be reached via.
        const std::string session_token(generate_path_uuid("", 't'));
        auto trq = TunnelRecord::Create(config_path, owner);
        queue[session_token] = trq;

        // Request the backend VPN client process to be started.
        if (!be_prxqry->CheckServiceAvail(Constants::GenInterface("backends")))
        {
            throw DBus::Exception(__func__,
                                  "Could not connect to net.openvpn.v3.backends");
        }
        auto be_start = DBus::Proxy::Client::Create(dbuscon,
                                                    Constants::GenServiceName("backends"));
        auto be_qry = DBus::Proxy::Utils::Query::Create(be_start);
        (void)be_qry->CheckObjectExists(Constants::GenPath("backends"),
                                        Constants::GenInterface("backends"));

        GVariant *r = be_start->Call(Constants::GenPath("backends"),
                                     Constants::GenInterface("backends"),
                                     "StartClient",
                                     glib2::Value::CreateTupleWrapped(session_token));
        g_variant_unref(r);

        // The session path for this session is returned
        return trq->session_path;
    }
    catch (const DBus::Exception &excp)
    {
        log->Debug("EXCEPTION [" + std::string(__func__) + "]: "
                   + std::string(excp.what()));
        throw DBus::Exception(__func__,
                              "Could not start the VPN client");
    }
}


void NewTunnelQueue::process_registration(DBus::Signals::Event::Ptr event)
{
    // std::cerr << __func__ << ":: " << event << std::endl;
    try
    {
        glib2::Utils::checkParams(__func__, event->params, "(ssi)");
        auto busn = glib2::Value::Extract<std::string>(event->params, 0);
        auto sesstok = glib2::Value::Extract<std::string>(event->params, 1);
        auto be_pid = glib2::Value::Extract<pid_t>(event->params, 2);

        // Look up the session token from the signal in the list of
        // new tunnels.  This gives access to the tunnel details needed
        // to start the VPN tunnel
        auto rec = queue.extract(sesstok);
        if (rec.empty())
        {
            log->LogCritical("Unknown session token recieved: " + sesstok);
            return;
        }
        // Get access to the TunnelRecord object
        auto tunnel = rec.mapped();

        // Create the session object which will be used to manage the
        // VPN session.  This is the bridge point betweeen the
        // end-users managing their session and the backend VPN client process
        auto session = object_mgr->CreateObject<Session>(dbuscon,
                                                         object_mgr,
                                                         creds_qry,
                                                         sesmgr_event,
                                                         tunnel->session_path,
                                                         tunnel->owner,
                                                         busn,
                                                         be_pid,
                                                         tunnel->config_path,
                                                         log->GetLogLevel(),
                                                         logwr);

        // Retrieve ACL details from the configuration manager for the
        // configuration profile this session uses
        auto cfgprx = OpenVPN3ConfigurationProxy(dbuscon, tunnel->config_path);
        try
        {
            if (cfgprx.GetTransferOwnerSession())
            {
                // If this configuration profile has the "transfer ownership"
                // flag set, the session owner will be moved from the user
                // starting the VPN session to the user owning the configuration
                // profile
                session->MoveToOwner(session->GetOwner(),
                                     cfgprx.GetOwner());
            }
        }
        catch (const DBus::Exception &excp)
        {
            log->LogCritical("Failed to move the session ownership from "
                             + lookup_username(session->GetOwner())
                             + " to " + lookup_username(cfgprx.GetOwner()));
        }


        // Prepare a registration confirmation to the VPN client process
        auto be_client = DBus::Proxy::Client::Create(dbuscon, busn);
        auto be_target = DBus::Proxy::TargetPreset::Create(Constants::GenPath("backends/session"),
                                                           Constants::GenInterface("backends"));
        try
        {
            GVariantBuilder *regdata = glib2::Builder::Create("(soo)");
            glib2::Builder::Add(regdata, rec.key());
            glib2::Builder::Add(regdata, tunnel->session_path);
            glib2::Builder::Add(regdata, tunnel->config_path);

            GVariant *reg = be_client->Call(be_target,
                                            "RegistrationConfirmation",
                                            glib2::Builder::Finish(regdata));
            glib2::Utils::checkParams(__func__, reg, "(s)");

            // The VPN client process responds with the configuration name
            // it has been requested to use
            auto config_name = glib2::Value::Extract<std::string>(reg, 0);

            // Update the session object with the config name; this is
            // static for this session object after this point
            session->SetConfigName(config_name);
            sesmgr_event->Send(tunnel->session_path,
                               EventType::SESS_CREATED,
                               tunnel->owner);

            log->Debug("New session confirmed - session-token=" + sesstok
                       + " client-bus-name=" + busn
                       + " client-pid=" + std::to_string(be_pid)
                       + " config-path=" + tunnel->config_path
                       + " config-name=" + config_name
                       + " session-path=" + tunnel->session_path);
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            log->LogCritical("Failed to complete session registration. "
                             "Backend process stopped (pid "
                             + std::to_string(be_pid) + ")");
            be_client->Call(be_target, "ForceShutdown", nullptr, true);
            object_mgr->RemoveObject(session->GetPath());
        }

        // Clean up and remove the tracking in the tunnel queue
        tunnel.reset();
        queue.erase(sesstok);
    }
    catch (const DBus::Exception &excp)
    {
        log->LogCritical("DBus::Exception - " + std::string(excp.GetRawError()));
    }
    catch (const std::exception &e)
    {
        log->LogCritical("EXCEPTION: " + std::string(e.what()));
    }
}

} // namespace SessionManager
