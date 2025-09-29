//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file log-service.hpp
 *
 *  @brief Declares the basic net.openvpn.v3.log service handler object and
 *         configuration setup.
 */


#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <syslog.h>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/service.hpp>

#include "dbus/constants.hpp"
#include "dbus/signals/log.hpp"
#include "dbus/signals/statuschange.hpp"
#include "common/utils.hpp"
#include "log-proxylog.hpp"
#include "logwriter.hpp"
#include "service-configfile.hpp"
#include "service-logger.hpp"


namespace LogService {

/**
 *  Initial configuration for the net.openvpn.v3.log service.  This
 *  is being populated after the command line arguments and optionally
 *  the configuration file has been parsed and merged into the ParsedArgs
 *  object.
 *
 *  The purpose of this is to preserve an initial state and have a simpler
 *  way to pass the configuration to the other objects needing this
 *  information
 */
struct Configuration
{
    LogServiceConfigFile::Ptr config_file = nullptr;
    LogService::Logger::Ptr servicelog = nullptr;
    Log::EventFilter::Ptr logfilter = nullptr;
    std::string log_file = "";
    std::string log_method = "";
    int32_t syslog_facility = LOG_DAEMON;
    bool log_dbus_details = false;
    bool log_prefix_logtag = true;
    bool log_timestamp = true;
    bool log_colour = false;
};

class AttachedService
{
  public:
    using Ptr = std::shared_ptr<AttachedService>;
    LogTag::Ptr logtag = nullptr;
    DBus::Signals::Target::Ptr src_target = nullptr;

    [[nodiscard]] static AttachedService::Ptr Create(
        DBus::Connection::Ptr connection,
        DBus::Object::Manager::Ptr obj_mgr,
        LogService::Logger::Ptr log,
        DBus::Signals::SubscriptionManager::Ptr submgr,
        LogTag::Ptr tag,
        const std::string &busname,
        const std::string &interface);
    ~AttachedService() noexcept;

    DBus::Object::Path AddProxyTarget(const std::string &recv_tgt,
                                      const DBus::Object::Path &session_path);

    void OverrideObjectPath(const DBus::Object::Path &new_path);

  private:
    DBus::Connection::Ptr connection = nullptr;
    DBus::Object::Manager::Ptr object_mgr = nullptr;
    LogService::Logger::Ptr log = nullptr;
    Signals::ReceiveLog::Ptr log_handler = nullptr;
    Signals::ReceiveStatusChange::Ptr status_handler = nullptr;
    std::map<DBus::Object::Path, std::shared_ptr<ProxyLogEvents>> proxies = {};
    DBus::Object::Path override_obj_path{};

    AttachedService(DBus::Connection::Ptr conn,
                    DBus::Object::Manager::Ptr obj_mgr,
                    LogService::Logger::Ptr log,
                    DBus::Signals::SubscriptionManager::Ptr submgr,
                    LogTag::Ptr tag,
                    const std::string &busname,
                    const std::string &interface);

    void process_log_event(const Events::Log &logevent);
    void process_statuschg_event(const std::string &sender,
                                 const DBus::Object::Path &path,
                                 const std::string &interface,
                                 const Events::Status &statuschg);
};

class ServiceHandler : public DBus::Object::Base
{
  public:
    ServiceHandler(DBus::Connection::Ptr connection,
                   DBus::Object::Manager::Ptr obj_mgr,
                   Configuration &&cfgobj);

    const bool Authorize(const DBus::Authz::Request::Ptr req) override;

  private:
    // Main service configuration and related objects/members
    DBus::Connection::Ptr connection = nullptr;
    DBus::Object::Manager::Ptr object_mgr = nullptr;
    Configuration config{};
    LogWriter::Ptr logwr = nullptr;
    LogService::Logger::Ptr log = nullptr;
    Log::EventFilter::Ptr logfilter = nullptr;
    DBus::Credentials::Query::Ptr dbuscreds = nullptr;
    DBus::Signals::SubscriptionManager::Ptr subscrmgr = nullptr;
    std::string version = package_version;

    // Log subscription related to D-Bus service subscription attachments
    using AttachMap = std::map<size_t, AttachedService::Ptr>;
    AttachMap log_attach_subscr{};
    std::mutex attachmap_mtx{};
    std::mutex method_detachmtx{};

    using SessionInterfKey = std::pair<DBus::Object::Path, std::string>;
    using SessionLogTagIndex = std::map<SessionInterfKey, size_t>;
    SessionLogTagIndex session_logtag_index{};


    /**
     *  D-Bus method: net.openvpn.v3.log.Attach
     *
     *  Requests the log service to listen for Log events from the
     *  calling remote D-Bus service.
     *
     *  Input:   (s)
     *    s - interface:  Log event's interface string to subscribe to
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_attach(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.log.AssignSession
     *
     *  Used by the net.openvpn.v3.backends.be$PID services to inform
     *  the log service about the session path it is assigned to use.
     *
     *  When end-users are requesting (via net.openvpn.v3.sessions) to
     *  receive log and status signals, the reference provided is the
     *  session path the VPN client session has been assigned.  This
     *  method provides the needed link to lookup the session path tox
     *  find which backend client it need to forward.
     *
     *  See method_proxy_log_events() for further details
     *
     *  Input:   (os)
     *    o - session_path: The session path the backend client is assigned
     *    s - interface:    The interface for the signals to consider for
     *                      forwarding
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_assign_session(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.log.Detach
     *
     *  Services who has attached to the logging session should clean up
     *  their attachment when it no longer wants to do any logging via
     *  this logging service.  This method will remove the signal subscription
     *  in this logging service from the calling remote D-Bus service.
     *
     *  Input:   (s)
     *    s - interface:   D-Bus object interface of the attached subscription
     *                     to unsubscribe from.
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_detach(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.log.GetSubscriberList
     *
     *  Provides a complete list of all the D-Bus services this logger is
     *  attached to and has subscribed to.
     *
     *  Output:  a(ssss)
     *    s - log_tag:     Unique LogTag string for the subscription, used
     *                     in the log events logged by this service
     *    s - busname:     Unique busname of the D-Bus service this logger
     *                     service is subscribed to
     *    s - interface:   D-Bus signal interface the subscription is
     *                     subscribed to
     *    s - path:        D-Bus object path the subscription will consider
     *                     for logging
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_get_subscr_list(DBus::Object::Method::Arguments::Ptr args);

    /**
     *  D-Bus method: net.openvpn.v3.log.ProxyLogEvents
     *
     *  This method is only available by the OPENVPN_USERNAME user and
     *  the net.openvpn.v3.sessions service.  End users can, via the session
     *  manager, request to receive log and signal events for a VPN session
     *  they have access to.
     *
     *  When the session manager calls this method, this logging service
     *  will send Log and StatusChange signals to the end-users D-Bus bus name
     *  for the requested session path.
     *
     *  Input:   (so)
     *    s - target_address:   Unique D-Bus name where to send signals
     *    o - session_path:     Session path assigned for the backend VPN client
     *                          to forward signals from
     *
     *  Output:  (o)
     *    o - proxy_path:       A path to a proxy object created by this logging
     *                          service.  This is used to further control the
     *                          this signal forwarding.  There is a proxy object
     *                          per proxy forwarding request
     *
     * @param args  DBus::Object::Method::Arguments
     */
    void method_proxy_log_events(DBus::Object::Method::Arguments::Ptr args);


    //
    // Internal methods
    //

    const bool check_busname_vpn_client(const std::string &caller) const;
    const bool check_busname_service_name(const std::string &caller,
                                          const std::string &busname) const;
    /**
     *  Remove a log subscription from the subscription list
     *
     *  NOTE: Call std::lock_guard<std::mutex> guard(attachmap_mtx) before
     *        calling this method to avoid failure in other functions
     *        accessing the subscription list in a parallel thread
     *
     * @param tag    LogTag of the subscription to remove
     * @param pid    pid of the caller. May be 0 as it is only used for logging
     * @param meta   LogMetaData object with details to attach to the log event
     */
    void remove_log_subscription(LogTag::Ptr tag,
                                 pid_t pid,
                                 LogMetaData::Ptr meta);
    const bool check_detach_access(LogTag::Ptr tag,
                                   const std::string &caller) const;
    void cleanup_service_subscriptions();

    /**
     *  Helper wrapper to handle saving changed properties to disk, if this
     *  has been enabled for this logging service
     *
     * @tparam T      Data type of the value for the property to modify
     * @param prop    DBus::Object::Property::BySpec object to the property to
     *                modify
     * @param key     Option/configuration key this property is related to
     * @param value   The new value for this property/setting
     *
     * @return DBus::Object::Property::Update::Ptr Update response for the
     *         D-Bus infrastructure
     */
    template <typename T>
    DBus::Object::Property::Update::Ptr save_property(const DBus::Object::Property::BySpec &prop,
                                                      const std::string &key,
                                                      T value)
    {
        try
        {
            if (config.config_file)
            {
                config.config_file->SetValue(key, value);
                config.config_file->Save();
            }
        }
        catch (const DBus::Exception &ex)
        {
            log->LogCritical("Failed saving property changes: "
                             + std::string(ex.what()));
        }

        std::ostringstream msg;
        msg << "Setting changed: " << key << " = " << value;
        log->LogVerb1(msg.str());

        auto upd = prop.PrepareUpdate();
        upd->AddValue(value);
        return upd;
    }
};


class MainService : public DBus::Service
{
  public:
    MainService(DBus::Connection::Ptr conn);
    void BusNameAcquired(const std::string &busname);
    void BusNameLost(const std::string &busname);
};

} // namespace LogService
