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
 * @file sessionmgr-session.hpp
 *
 * @brief Declaration of the Session Manager session object
 */

#pragma once

#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/object/manager.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>
#include <gdbuspp/proxy.hpp>

#include "dbus/object-ownership.hpp"
#include "log/proxy-log.hpp"
#include "log/logwriter.hpp"
#include "common/requiresqueue.hpp"
#include "dbus/signals/attention-required.hpp"
#include "dbus/signals/statuschange.hpp"
#include "sessionmgr-signals.hpp"


namespace SessionManager {

struct SessionProperties
{
    uid_t owner;
    std::time_t session_created;
};


enum class DCOstatus : unsigned short
{
    UNCHANGED,
    MODIFIED,
    LOCKED
};


class Session : public DBus::Object::Base
{
  public:
    Session(DBus::Connection::Ptr dbuscon,
            DBus::Object::Manager::Ptr objmgr,
            DBus::Credentials::Query::Ptr creds_qry,
            ::Signals::SessionManagerEvent::Ptr sig_sessionmgr,
            const DBus::Object::Path &sespath,
            const uid_t owner,
            const std::string &be_busname,
            const pid_t be_pid,
            const DBus::Object::Path &cfg_path,
            const unsigned int loglev,
            LogWriter::Ptr logwr);
    ~Session() noexcept;

    /**
     *  Changes the session's configuration name property (config_name)
     *
     *  This is called by the RegistrationRequest
     * @param cfg_name
     */
    void SetConfigName(const std::string &cfg_name);
    const std::string GetConfigName() const noexcept;
    const std::string GetDeviceName() const noexcept;

    const bool CheckACL(const std::string &caller) const noexcept;
    const uid_t GetOwner() const noexcept;
    void MoveToOwner(const uid_t from_uid, const uid_t to_uid);

  protected:
    const bool Authorize(DBus::Authz::Request::Ptr) override;
    const std::string AuthorizationRejected(const Authz::Request::Ptr) const noexcept override;

  private:
    using LogProxyList = std::map<std::string, LogProxy::Ptr>;

    DBus::Connection::Ptr dbus_conn = nullptr;
    DBus::Object::Manager::Ptr object_mgr = nullptr;
    DBus::Credentials::Query::Ptr creds_qry = nullptr;
    ::Signals::SessionManagerEvent::Ptr sig_sessmgr = nullptr;
    pid_t backend_pid = -1;
    DBus::Object::Path config_path = {};
    std::string config_name{};
    RequiresQueue::Ptr req_queue = nullptr;
    Log::Ptr sig_session = nullptr;
    DBus::Signals::SubscriptionManager::Ptr sigsubscr = nullptr;
    DBus::Signals::Emit::Ptr broadcast_emitter = nullptr;
    ::Signals::AttentionRequired::Ptr sig_attreq = nullptr;
    ::Signals::StatusChange::Ptr sig_statuschg = nullptr;
    GDBusPP::Object::Extension::ACL::Ptr object_acl;
    bool restrict_log_access = true;
    LogProxyList log_forwarders = {};
    std::time_t created = std::time(nullptr);
    DBus::Proxy::Client::Ptr be_prx = nullptr;
    DBus::Proxy::TargetPreset::Ptr be_target = nullptr;
    DCOstatus dco_status = DCOstatus::UNCHANGED;
    bool dco = false;
    bool connection_started = false;

    /**
     *  D-Bus method: net.openvpn.v3.sessions.Ready
     *      Checks if the VPN tunnel is ready to be started or if it needs
     *      more information from the VPN session owner.
     *
     *  Input:   n/a
     *  Output:  n/a
     *
     *  Throws: net.openvpn.v3.error.ready with a human readable string
     *          if the session owner need to provide more information.
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_ready(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  Generic backend VPN client method callback handler
     *
     *  D-Bus method: net.openvpn.v3.sessions.Restart
     *      Restarts the VPN session
     *  Input:   n/a
     *  Output:  n/a
     *
     *  D-Bus method: net.openvpn.v3.sessions.Pause
     *      Pauses a VPN session, which results in a stateful disconnect.
     *      The VPN backend client disconnects competely and closes the session
     *      but the client process can be resumed later on to re-establish the
     *      tunnel quickly
     *  Input:   (s)
     *      s - reason: A brief explantion of why the session was paused
     *  Output:  n/a
     *
     *  D-Bus method: net.openvpn.v3.sessions.Resume
     *      Resumes a paused VPN session
     *  Input:   n/a
     *  Output:  n/a
     *
     *  D-Bus method: net.openvpn.v3.sessions.Resume
     *      Resumes a paused VPN session
     *  Input:   n/a
     *  Output:  n/a
     *
     * @param args          DBus::Object::Method::Arguments
     * @param method        std::string with the method name (without interface)
     *                      to call in the backend client service
     * @param no_response   bool flag, if the result from the backend should
     *                      be passed on to the caller (false) or just being
     *                      ignored (true)
     */
    void method_proxy_be(DBus::Object::Method::Arguments::Ptr args,
                         const std::string &method,
                         const bool no_response);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.Connect
     *      Tells the backend VPN client service to start connecting to the
     *      configured VPN servers
     *
     *  Input:   n/a
     *  Output:  n/a
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_connect(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.LogForward
     *      Enables or disables Log and StatusChange events from the
     *      backend VPN client service to the caller of this method.  This
     *      will be handled by the net.openvpn.v3.log service, which will
     *      do a unicast to the bus name of the caller to this method.
     *
     *      Each enabled log forwarding will be assigned a log proxy path
     *      within the net.openvpn.v3.log service.  These paths are also
     *      tracked in the the log_forwards property in the
     *      SessionManager::Session object.
     *
     *  Input:   (b)
     *      b - enable: Bool flag to enable or disable the log forwarding.
     *  Output:  n/a
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_log_forward(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.AccessGrant
     *      Adds a user to the ACL list who can access and manage this
     *      Session object
     *
     *  Input:   (u)
     *      u - uid - Unix UID of the user to grant access
     *  Output:  n/a
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_access_grant(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.AccessRevoke
     *      Removes a user from the ACL list who can access and manage this
     *      Session object
     *
     *  Input:   (u)
     *      u - uid - Unix UID of the user to revoke access from
     *  Output:  n/a
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_access_revoke(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  Generic method to close and shutdown the VPN session
     *
     *  This will request the backend VPN process to shut down and
     *  delete the Session object in the Session Manager.
     *
     *  This method is also used by the Disconnect D-Bus method
     *
     *  D-Bus method: net.openvpn.v3.sessions.Disconnect
     *  Input:   n/a
     *  Output:  n/a
     *
     * @param forced  boolean flag.  If true, it will use the ForceShutdown
     *                method in the backend VPN client which will not do a
     *                clean disconnect and shutdown.  Normally this should
     *                be called with the forced flag being false, which
     *                ensures a clean disconnect and shutdown.
     */
    void close_session(const bool forced);


    void helper_stop_log_forwards();
};


} // namespace SessionManager
