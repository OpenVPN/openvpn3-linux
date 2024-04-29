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
 * @file sessionmgr-service.hpp
 *
 * @brief Declaration of the net.openvpn.v3.sessions D-Bus service
 */

#include <functional>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/service.hpp>

#include "common/utils.hpp"
#include "dbus/constants.hpp"
#include "log/logwriter.hpp"
#include "log/proxy-log.hpp"
#include "sessionmgr-session.hpp"
#include "sessionmgr-signals.hpp"
#include "tunnel-queue.hpp"

using namespace DBus;

namespace SessionManager {

using SessionCollection = std::vector<std::shared_ptr<Session>>;


/**
 *  The service handler object is the root Session Manager object which
 *  is used to create new VPN tunnels and the related session object, in
 *  addition provide various generic management interfaces for the service
 *  itself.
 *
 *  This object is created by the SessionManager::Service object when
 *  the Session Manager bus name has been acquired.
 */
class SrvHandler : public Object::Base
{
  public:
    SrvHandler(DBus::Connection::Ptr con,
               Object::Manager::Ptr objmgr,
               LogWriter::Ptr lwr);
    ~SrvHandler() = default;

    /**
     *  Changes the log level of the current Session Manager service
     *
     * @param loglvl
     */
    void SetLogLevel(const uint8_t loglvl);

  protected:
    /**
     *  Authorization callback for D-Bus object access.
     *
     *  The service handler does not have any authorziation checks, so
     *  it will always return true.
     *
     * @param request   Authz::Request object
     * @return Will always return true for the service handler object
     */
    const bool Authorize(const Authz::Request::Ptr request) override;


  private:
    Connection::Ptr dbuscon = nullptr;
    Object::Manager::Ptr object_mgr = nullptr;
    LogWriter::Ptr logwr = nullptr;
    DBus::Credentials::Query::Ptr creds_qry = nullptr;
    std::string version{package_version};
    SessionManager::Log::Ptr sig_sessmgr = nullptr;
    DBus::Signals::Emit::Ptr broadcast_emitter = nullptr;
    Signals::SessionManagerEvent::Ptr sig_sessmgr_event = nullptr;
    std::shared_ptr<NewTunnelQueue> tunnel_queue = nullptr;


    /**
     *  D-Bus method: net.openvpn.v3.sessions.NewTunnel
     *  Creates a new VPN tunnel session
     *
     *  Input:   (o)
     *    o - config_path: D-Bus object path to configuration profile
     *
     *  Output:  (o)
     *    o - session_path: D-Bus object path to the session path in this
     *                          session manager
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_new_tunnel(Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.FetchAvailableSessions
     *  Retrieve a list of object paths to accessible VPN sessions.  Only
     *  sessions available to the calling user will be provided.
     *
     *  Input:   n/a
     *
     *  Output:  (ao)
     *    ao - paths: Array of object paths of all accessible session objects
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_fetch_avail_sessions(Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.FetchManagedInterfaces
     *  Retrieve a list of virtual interface names in use by the calling
     *  user.

     *  Input:   n/a
     *
     *  Output:  (as)
     *    as - devices: Array of strings with the virtual interface names
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_fetch_managed_interf(Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.LookupConfigName
     *  Retrieve a list of VPN session objects which was started by a given
     *  configuration profile name.  Only VPN sessions available to the calling
     *  user will be returned
     *
     *  Input: (s)
     *    s - config_name: String with the configuration profile name to
     *                     search for
     *
     *  Output: (ao)
     *    ao - paths: Array of object paths matching the configuration
     *                profile name
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_lookup_config(Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.sessions.LookupInterface
     *  Retrieve the VPN session path used by a specific virtual interface
     *  name.  This will only return a single object path, since there must
     *  be a 1:1 relation between the interface name and the session object.
     *
     *  The calling user must have access to this session object to get a
     *  valid result back.
     *
     *  Input: (s)
     *    s - device_name: String with the virtual interface name to look up
     *
     *  Output: (o)
     *    o - session_path: D-Bus object path to the related VPN session
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_lookup_interf(Object::Method::Arguments::Ptr args);

    /**
     *  The helper_retrieve_sessions() method does the specific filtering
     *  via a search filter function.  This is a simple lambda function
     *  which will receive the SubscriptionManager::Session object to
     *  evaluate.  If this is to be included in the search result, the
     *  lambda need to return true.  It will be skipped if the function
     *  returns false.
     */
    using fn_search_filter = std::function<bool(std::shared_ptr<Session>)>;

    /**
     *  Internal helper method which retrieves a collection of session
     *  objects based on the evaluation of a search filter.
     *
     *  The DBus::Object::Manager will be queried to retrieve all D-Bus
     *  objects it manages on this objects behalf.  The "root object"
     *  (aka Service Handler) object is excluded from the search result.
     *  Only SessionManager::Session objects will be returned
     *
     *  The search filter (described in @fn_search_filter) is a simple
     *  lambda function which receives the Session object as the only argument
     *  and can do a decision to include it in the final result or not.
     *
     * @param caller              D-Bus caller's unique bus name, used to
     *                            only include Session object accessible to
     *                            the caller.  If this is empty, no ACL
     *                            checks will be done.
     *
     * @param filter_fn           Lambda search filter function, see
     *                            @fn_search_filter for more details
     *
     * @return SessionCollection  A std::vector<Session> of all session
     *         objects qualified by the search criterias (caller and
     *         search filter)
     */
    SessionCollection helper_retrieve_sessions(const std::string &caller,
                                               fn_search_filter &&filter_fn) const;
};



/**
 *  The net.openvpn.v3.sessions service manages all OpenVPN 3 VPN
 *  client sessions on the system.  This service is the main interface
 *  for all users who is granted privileges to start and manage VPN sessions
 *
 *  This service will interact with backend services which are restricted
 *  and inaccessible directly for end users.
 *
 */
class Service : public DBus::Service
{
  public:
    Service(DBus::Connection::Ptr con, LogWriter::Ptr logwr);

    ~Service() noexcept;

    void BusNameAcquired(const std::string &busname) override;
    void BusNameLost(const std::string &busname) override;

    void SetLogLevel(const uint8_t loglvl);

  private:
    LogWriter::Ptr logwr = nullptr;
    LogServiceProxy::Ptr logsrvprx = nullptr;
    uint8_t log_level = 3;
};



} // namespace SessionManager
