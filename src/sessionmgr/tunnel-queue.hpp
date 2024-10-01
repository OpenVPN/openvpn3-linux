//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

/**
 *  @file tunnel-queue.hpp
 *
 *  @brief  Internal queuing mechanism for net.openvpn.v3.sessions.NewTunnel
 *          handling.  When a new tunnel is requested, the Session Manager
 *          will request a new backend client process to be started.  This
 *          is handled by the tunnel queueing which creates the a new
 *          SessionManager::Session object per tunnel instance once the
 *          backend client process is ready.
 */

#pragma once
#include <ctime>
#include <map>
#include <optional>
#include <set>
#include <gdbuspp/bus-watcher.hpp>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/manager.hpp>
#include <gdbuspp/object/path.hpp>

#include "dbus/constants.hpp"
#include "dbus/path.hpp"
#include "sessionmgr-signals.hpp"


namespace SessionManager {

/**
 *  Contains details about a single requested new tunnel.
 *
 *  This information is used when creating the new SessionManager::Session
 *  object when the backend client signals it is ready.
 *
 */
struct TunnelRecord
{
  public:
    using Ptr = std::shared_ptr<TunnelRecord>;

    static TunnelRecord::Ptr Create(const DBus::Object::Path &cfgpath,
                                    const uid_t owner_uid,
                                    const std::optional<std::string> &session_path = std::nullopt);


    const std::time_t created = std::time(nullptr);
    const DBus::Object::Path session_path;
    const DBus::Object::Path config_path;
    const uid_t owner;

  private:
    TunnelRecord(const DBus::Object::Path &cfgpath,
                 const uid_t owner_uid,
                 const std::optional<std::string> &session_path);
};

/**
 * Maps the random session token for a new tunnel with a TunnelRecord object
 * containing the details for that session
 */
using QueuedTunnels = std::map<std::string, std::shared_ptr<TunnelRecord>>;


/**
 *  This class will keep track of all newly requested VPN tunnels, via
 *  the net.openvpn.v3.sessions.NewTunnel D-Bus method.
 *
 *  When a new tunnel is added to the queue, it wil call the
 *  net.openvpn.v3.backends.StartClient method (openvpn3-service-backendstart).
 *  This service will start an openvpn3-service-client process (the VPN client)
 *  which will send a "RegistrationRequest" signal to this SessionManager
 *
 *  This object will listen for the "RegistrationRequest" signals and will
 *  create the Session object and complete the registration.  This establishes
 *  a 1:1 link between the openvpn3-service-client process with the Session
 *  object managed in this process.
 */
class NewTunnelQueue
{
  public:
    using Ptr = std::shared_ptr<NewTunnelQueue>;

    [[nodiscard]] static NewTunnelQueue::Ptr Create(DBus::Connection::Ptr dbuscon,
                                                    DBus::Credentials::Query::Ptr creds_qry,
                                                    DBus::Object::Manager::Ptr objmgr,
                                                    LogWriter::Ptr logwr,
                                                    SessionManager::Log::Ptr sig_log,
                                                    ::Signals::SessionManagerEvent::Ptr sesmgrev);

    /**
     *  Enqueues a new tunnel request to the queue
     *
     *  This method will generate both a session token which is sent
     *  to the openvpn3-service-client process and a D-Bus session path which
     *  will be returned to the caller
     *
     *  NOTE: The returned D-Bus path will not be available (visible) before
     *        the client process has registered itself with this service.
     *
     * @param config_path         std::string with the D-Bus configuration path
     * @param owner               uid_t of the owner of the session
     * @return const DBus::Object::Path with the assigned session path
     */
    const DBus::Object::Path AddTunnel(const std::string &config_path,
                                       const uid_t owner,
                                       const std::optional<std::string> &existing_session_path = std::nullopt);

  private:
    DBus::Connection::Ptr dbuscon = nullptr;
    DBus::Credentials::Query::Ptr creds_qry = nullptr;
    DBus::Object::Manager::Ptr object_mgr = nullptr;
    LogWriter::Ptr logwr = nullptr;
    SessionManager::Log::Ptr log = nullptr;
    ::Signals::SessionManagerEvent::Ptr sesmgr_event = nullptr;
    DBus::Proxy::Utils::DBusServiceQuery::Ptr be_prxqry = nullptr;
    DBus::Signals::SubscriptionManager::Ptr signal_subscr = nullptr;
    DBus::Signals::Target::Ptr subscr_target = nullptr;
    QueuedTunnels queue{};
    std::map<std::string, BusWatcher::Ptr> backend_watchers;
    std::set<std::string> expired_backend_watchers;

    /**
     *  Callback function triggered when the backend VPN client
     *  (openvpn3-service-client) sends the RegistrationRequest signal.
     *
     *  This will create and populate the SessionManager::Session object
     *  and registered it as a D-Bus object owned by this session manager
     *  service.  Once the D-Bus object is created, it will call the
     *  finalizing RegistrationConfirmation call to the backend VPN client.
     *
     *  Once this is done successfully, the session object is ready to be
     *  accessed.
     *
     * @param event  The DBus::Signals::Event object with the signal details
     */
    void process_registration(DBus::Signals::Event::Ptr event);


    NewTunnelQueue(DBus::Connection::Ptr dbuscon,
                   DBus::Credentials::Query::Ptr creds_qry,
                   DBus::Object::Manager::Ptr objmgr,
                   LogWriter::Ptr logwr,
                   SessionManager::Log::Ptr sig_log,
                   ::Signals::SessionManagerEvent::Ptr sesmgrev);
};

} // namespace SessionManager
