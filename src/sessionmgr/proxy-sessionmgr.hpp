//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   proxy-sessionmgr.hpp
 *
 * @brief  Provides a C++ object implementation of a D-Bus session manger
 *         and session object.  This proxy class will perform the D-Bus
 *         calls and provide the results as native C++ data types.
 */

#pragma once

#include <condition_variable>
#include <iostream>
#include <memory>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "build-config.h"

#ifdef HAVE_TINYXML
#include <tinyxml2.h>
#include <openvpn/common/exception.hpp>
#include <openvpn/common/xmlhelper.hpp>
using XmlDocPtr = std::shared_ptr<openvpn::Xml::Document>;
#endif


#include "dbus/requiresqueue-proxy.hpp"
#include "client/statistics.hpp"
#include "common/utils.hpp"
#include "events/status.hpp"
#include "log/log-helpers.hpp"
#include "log/dbus-log.hpp"
#include "sessionmgr-events.hpp"

namespace SessionManager::Proxy {


class Exception : public DBus::Exception
{
  public:
    Exception(const std::string &err) noexcept
        : DBus::Exception("SessionManager::Proxy", err)
    {
    }
};

/**
 * This exception is thrown when the Session::Ready() call
 * indicates the VPN backend client needs more information from the
 * frontend process.
 */
class ReadyException : public SessionManager::Proxy::Exception
{
  public:
    ReadyException(const std::string &err) noexcept
        : SessionManager::Proxy::Exception(err)
    {
    }
};


/**
 *  This is thrown when there are issues looking up a virtual interface name
 */
class TunInterfaceException : public SessionManager::Proxy::Exception
{
  public:
    TunInterfaceException(const std::string &err)
        : SessionManager::Proxy::Exception(err)
    {
    }
};



/**
 *  Container object for connection details of the server a session
 *  is connected to
 */
struct ConnectedToDetails
{
    /**
     *  Construct and populate a new ConnectedToDetails object
     *
     * @param proto      std::string, protocol
     * @param srv_ip     std::string, server IP address
     * @param srv_port   uint32_t server port client is connected to
     */
    ConnectedToDetails(const std::string &proto,
                       const std::string &srv_ip,
                       uint32_t srv_port)
        : protocol(proto), server_ip(srv_ip), server_port(srv_port)
    {
    }

    /// Protocol the connection uses, udp/tcp, udp-dco/tcp-dco
    const std::string protocol;

    /// IP address of the VPN server the client is connected with
    const std::string server_ip;

    /// UDP/TCP port the client is connected to
    const uint32_t server_port;
};



/**
 *  Client proxy implementation interacting with a
 *  SessionObject in the session manager over D-Bus
 */
class Session : public DBusRequiresQueueProxy
{
  public:
    using Ptr = std::shared_ptr<Session>;
    using List = std::vector<Session::Ptr>;

    [[nodiscard]] static Ptr Create(DBus::Proxy::Client::Ptr prx,
                                    const DBus::Object::Path &objpath)
    {
        return Ptr(new Session(prx, objpath));
    };


    bool CheckSessionExists() const noexcept
    {
        return prxqry->CheckObjectExists(target->object_path,
                                         target->interface);
    }


    const DBus::Object::Path GetPath() const
    {
        return target->object_path;
    }

    /**
     *  Retrieve the PID of the openvpn3-service-client process this
     *  session object is assigned to
     *
     * @return pid_t
     */
    pid_t GetBackendPid() const
    {
        return proxy->GetProperty<uint32_t>(target, "backend_pid");
    }

    /**
     *  Makes the VPN backend client process start the connecting to the
     *  VPN server
     */
    void Connect()
    {
        simple_call("Connect", "Failed to start a new tunnel");
    }


    /**
     *  Makes the VPN backend client process disconnect and then
     *  instantly reconnect to the VPN server
     */
    void Restart()
    {
        simple_call("Restart", "Failed to restart tunnel");
    }


    /**
     *  Disconnects and shuts down the VPN backend process.  This call
     *  will invalidate the current session object.  This can also be used
     *  to shutdown a backend process before doing a Connect() call.
     */
    void Disconnect()
    {
        simple_call("Disconnect", "Failed to disconnect tunnel");
    }


    /**
     *  Pause an on-going VPN tunnel.  Pausing and Resuming an existing VPN
     *  tunnel is generally much faster than doing a full Disconnect() and
     *  Connect() cycle.
     *
     * @param reason  A string provided to the VPN backend process why the
     *                tunnel was suspended.  Primarily used for logging.
     */
    void Pause(const std::string &reason)
    {
        try
        {
            GVariant *res = proxy->Call(target,
                                        "Pause",
                                        glib2::Value::CreateTupleWrapped(reason));
            g_variant_unref(res);
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("Failed to pause the session");
        }
    }


    /**
     *   Resumes a paused VPN tunnel
     */
    void Resume()
    {
        simple_call("Resume", "Failed to resume tunnel");
    }


    /**
     *  Checks if the VPN backend process has all it needs to start connecting
     *  to a VPN server.  If it needs more information from the front-end, a
     *  ReadyException will be thrown with more details.
     */
    void Ready()
    {
        try
        {
            GVariant *res = proxy->Call(target, "Ready");
            g_variant_unref(res);
        }
        catch (DBus::Proxy::Exception &excp)
        {
            // Throw D-Bus errors related to "Ready" errors as ReadyExceptions
            std::string e(excp.what());
            if (e.find("net.openvpn.v3.error.ready") != std::string::npos)
            {
                throw ReadyException(excp.GetRawError());
            }
            // Otherwise, just rethrow the DBusException
            throw SessionManager::Proxy::Exception(excp.what());
        }
    }


    /**
     * Retrieves the last reported status from the VPN backend
     *
     * @return  Returns a populated struct StatusEvent with the full status.
     */
    Events::Status GetLastStatus()
    {
        GVariant *status = proxy->GetPropertyGVariant(target, "status");
        Events::Status ret(status);
        g_variant_unref(status);
        return ret;
    }


    /**
     *  Will the session log properties be accessible to users granted
     *  access to the session?
     *
     * @return  Returns false if users can modify receive_log_events
     *          and the log_verbosity properties.  True means only the
     *          session owner has access to the session log via the session
     *          manager.
     */
    const bool GetRestrictLogAccess()
    {
        return proxy->GetProperty<bool>(target, "restrict_log_access");
    }


    /**
     *  Change who can access the session log related properties.
     *
     * @param enable  If false, users can modify receive_log_events
     *                and the log_verbosity properties.  True means only the
     *                session owner has access to the session log via the
     *                session manager.
     */
    void SetRestrictLogAccess(bool enable)
    {
        proxy->SetProperty(target, "restrict_log_access", enable);
    }


    /**
     *  Will the VPN client backend send log messages via
     *  the session manager?
     *
     * @return  Returns true if session manager will proxy log events from
     *          the VPN client backend
     */
    const bool GetReceiveLogEvents()
    {
        return proxy->GetProperty<bool>(target, "receive_log_events");
    }


    /**
     *  Change the session manager log event proxy
     *
     * @param enable  If true, the session manager will proxy log events
     */
    void SetReceiveLogEvents(bool enable)
    {
        proxy->SetProperty(target, "receive_log_events", enable);
    }


    /**
     *  Get the log verbosity of the log messages being proxied
     *
     * @return  Returns an integer between 0 and 6,
     *          where 6 is the most verbose.  With 0 only fatal and critical
     *          errors will be provided
     */
    const uint32_t GetLogVerbosity()
    {
        return proxy->GetProperty<uint32_t>(target, "log_verbosity");
    }


    /**
     *  Sets the log verbosity level of proxied log events
     *
     * @param loglevel  An integer between 0 and 6, where 6 is the most
     *                  verbose and 0 the least.
     */
    void SetLogVerbosity(uint32_t loglevel)
    {
        proxy->SetProperty(target, "log_verbosity", loglevel);
    }


    /**
     * Retrieve the last log event which has been saved
     *
     * @return Returns a populated struct LogEvent with the the complete log
     *         event.
     */
    const Events::Log GetLastLogEvent()
    {
        GVariant *logev = proxy->GetPropertyGVariant(target, "last_log");
        auto ret = Events::ParseLog(logev);
        g_variant_unref(logev);
        return ret;
    }


    /**
     * Retrieves statistics of a running VPN tunnel.  It is gathered by
     * retrieving the 'statistics' session object property.
     *
     * @return Returns a ConnectionStats (std::vector<ConnectionStatDetails>)
     *         array of all gathered statistics.
     */
    ConnectionStats GetConnectionStats()
    {
        GVariant *statsprops = proxy->GetPropertyGVariant(target, "statistics");
        GVariantIter *stats_ar = nullptr;
        g_variant_get(statsprops, "a{sx}", &stats_ar);

        ConnectionStats ret;
        GVariant *r = nullptr;
        while ((r = g_variant_iter_next_value(stats_ar)))
        {
            gchar *key = nullptr;
            gint64 val;
            g_variant_get(r, "{sx}", &key, &val);
            ret.push_back(ConnectionStatDetails(std::string(key), val));
            g_variant_unref(r);
            g_free(key);
        }
        g_variant_iter_free(stats_ar);
        g_variant_unref(statsprops);

        return ret;
    }


    /**
     *  Manipulate the public-access flag.  When public-access is set to
     *  true, everyone have access to this session regardless of how the
     *  access list is configured.
     *
     *  @param public_access Boolean flag.  If set to true, everyone is
     *                       granted read access to the session.
     */
    void SetPublicAccess(bool public_access)
    {
        proxy->SetProperty(target, "public_access", public_access);
    }


    /**
     *  Retrieve the public-access flag for session
     *
     * @return Returns true if public-access is granted
     */
    const bool GetPublicAccess()
    {
        return proxy->GetProperty<bool>(target, "public_access");
    }


    /**
     * Grant a user ID (uid) access to this session
     *
     * @param uid  uid_t value of the user which will be granted access
     */
    void AccessGrant(uid_t uid)
    {
        try
        {
            GVariant *res = proxy->Call(target,
                                        "AccessGrant",
                                        glib2::Value::CreateTupleWrapped(uid));
            g_variant_unref(res);
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("AccessGrant() call failed");
        }
    }


    /**
     * Revoke the access from a user ID (uid) for this session
     *
     * @param uid  uid_t value of the user which will get access revoked
     */
    void AccessRevoke(uid_t uid)
    {
        try
        {
            GVariant *res = proxy->Call(target,
                                        "AccessRevoke",
                                        glib2::Value::CreateTupleWrapped(uid));
            g_variant_unref(res);
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("AccessRevoke() call failed");
        }
    }


    /**
     *  Enable/Disable the LogEvent forwarding from the client backend
     *
     * @param enable  bool value to enable or disable the forwarding
     */
    void LogForward(bool enable)
    {
        try
        {
            GVariant *res = proxy->Call(target,
                                        "LogForward",
                                        glib2::Value::CreateTupleWrapped(enable));
            g_variant_unref(res);
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("LogForward() call failed");
        }
    }


    /**
     *  Retrieve the owner UID of this session object
     *
     * @return Returns uid_t of the session object owner
     */
    const uid_t GetOwner()
    {
        return proxy->GetProperty<uid_t>(target, "owner");
    }


    /**
     *  Retrieve the complete access control list (acl) for this object.
     *  The acl is essentially just an array of user ids (uid)
     *
     * @return Returns an array if uid_t references for each user granted
     *         access.
     */
    const std::vector<uid_t> GetAccessList()
    {
        auto ret = proxy->GetPropertyArray<uid_t>(target, "acl");
        return ret;
    }


    /**
     *  Retrieve the network interface name used for this tunnel
     *
     * @return  Returns a std::string with the device name
     */
    const std::string GetDeviceName() const
    {
        return proxy->GetProperty<std::string>(target, "device_name");
    }


    /**
     *  Retrieve if the session is configured to use a Data Channel Offload (DCO)
     *  interface
     *
     * @return  Returns true if DCO is enabled
     */
    const bool GetDCO() const
    {
        return proxy->GetProperty<bool>(target, "dco");
    }


    /**
     *  Enable or disable the Data Channel Offload (DCO) interface.  This can
     *  only be called _before_ starting a connection.  Otherwise it will throw
     *  an exception.
     *
     * @param dco  Boolean, true to enable DCO
     *
     */
    void SetDCO(bool dco) const
    {
        proxy->SetProperty(target, "dco", dco);
    }


    /**
     *  Retrieve the configuration profile name used when the session
     *  was started.
     *
     *  If the configuration name has been changed in the configuration
     *  manager after the session was started, this method will still return
     *  the old configuration name prior the name change.
     *
     * @return std::string with the configuration profile name
     */
    std::string GetConfigName() const
    {
        return proxy->GetProperty<std::string>(target, "config_name");
    }


    /**
     *  Retrieve the configuration profile D-Bus path for the session
     *
     * @return DBus::Object::Path
     */
    DBus::Object::Path GetConfigPath() const
    {
        return proxy->GetProperty<DBus::Object::Path>(target, "config_path");
    }


    /**
     *  Retrieve the assigned session name
     *
     *  Once the connection has been established, the OpenVPN 3 Core Library
     *  will assign the session a session name.
     *
     * @return std::string
     */
    std::string GetSessionName() const
    {
        return proxy->GetProperty<std::string>(target, "session_name");
    }


    /**
     *  Retrieve information about the server this session is connected to
     *
     *  The collected information will be provided in a ConnectedToDetails
     *  object.
     *
     * @return ConnectedToDetails
     * @throws SessionManager::Proxy::Exception if information is accessible
     */
    ConnectedToDetails GetConnectedToInfo() const
    {
        try
        {
            GVariant *cd = proxy->GetPropertyGVariant(target, "connected_to");
            glib2::Utils::checkParams(__func__, cd, "(ssu)");
            ConnectedToDetails ret{
                glib2::Value::Extract<std::string>(cd, 0),
                glib2::Value::Extract<std::string>(cd, 1),
                glib2::Value::Extract<uint32_t>(cd, 2)};
            return ret;
        }
        catch (const DBus::Exception &)
        {
            throw SessionManager::Proxy::Exception("Connection details are not available");
        }
    }


    /**
     *  Get a localized string with the timestamp of when the session was
     *  started.
     *
     * @return std::string
     */
    std::string GetSessionCreated() const
    {
        std::time_t sess_created = proxy->GetProperty<uint64_t>(target,
                                                                "session_created");
        return get_local_tstamp(sess_created);
    }


  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr target = nullptr;
    DBus::Proxy::Utils::Query::Ptr prxqry = nullptr;

    Session(DBus::Proxy::Client::Ptr prx, const DBus::Object::Path &objpath)
        : DBusRequiresQueueProxy("UserInputQueueGetTypeGroup",
                                 "UserInputQueueFetch",
                                 "UserInputQueueCheck",
                                 "UserInputProvide"),
          proxy(prx)
    {
        target = DBus::Proxy::TargetPreset::Create(objpath,
                                                   Constants::GenInterface("sessions"));
        prxqry = DBus::Proxy::Utils::Query::Create(proxy);
        AssignProxy(proxy, target);
    }


    void simple_call(const std::string &method, const std::string &errmsg)
    {
        try
        {
            GVariant *r = proxy->Call(target, method, nullptr);
            g_variant_unref(r);
        }
        catch (const DBus::Proxy::Exception &excp)
        {
            throw SessionManager::Proxy::Exception(excp.what());
        }
    }
};



class Manager
{
  public:
    using Ptr = std::shared_ptr<Manager>;

    [[nodiscard]] static Ptr Create(DBus::Connection::Ptr conn)
    {
        return Ptr(new Manager(conn));
    };

#ifdef HAVE_TINYXML
    XmlDocPtr Introspect()
    {
        auto prxqry = DBus::Proxy::Utils::Query::Create(proxy);
        std::string introsp = prxqry->Introspect(Constants::GenPath("sessions"));
        XmlDocPtr doc;
        doc.reset(new openvpn::Xml::Document(introsp, "introspect"));
        return doc;
    }
#endif


    /**
     *
     * @param cfgpath  VPN profile configuration D-Bus path to use for the
     *                 backend client
     * @return Returns an OpenVPN3SessionProxy::Ptr to the new session
     *         D-Bus object
     */
    Session::Ptr NewTunnel(const DBus::Object::Path &cfgpath)
    {
        using namespace std::literals;

        try
        {
            auto signal_subscr = DBus::Signals::SubscriptionManager::Create(dbuscon);
            auto subscr_target = DBus::Signals::Target::Create(
                "",
                Constants::GenPath("sessions"),
                "");

            bool sessmgr_event = false;
            std::mutex new_tunnel_mtx;
            std::condition_variable new_tunnel_cv;
            std::string session_path;
            signal_subscr->Subscribe(subscr_target,
                                     "SessionManagerEvent",
                                     [&](DBus::Signals::Event::Ptr event)
                                     {
                                         std::lock_guard signal_lock{new_tunnel_mtx};
                                         SessionManager::Event ev(event->params);
                                         if (SessionManager::EventType::SESS_CREATED == ev.type
                                             && ev.path == session_path)
                                         {
                                             sessmgr_event = true;
                                             new_tunnel_cv.notify_all();
                                         }
                                     });
            {
                // This blocks the signal handler from trying to check
                // the session_path before it's been set.
                std::unique_lock<std::mutex> lock{new_tunnel_mtx};
                GVariant *r = proxy->Call(target,
                                          "NewTunnel",
                                          glib2::Value::CreateTupleWrapped(cfgpath));
                session_path = glib2::Value::Extract<DBus::Object::Path>(r, 0);
            }

            // Wait for the SessionManagerEvent for our session to arrive
            // before trying to create the Session object.
            std::unique_lock<std::mutex> lock{new_tunnel_mtx};
            bool res = new_tunnel_cv.wait_for(lock, 15s, [&]
                                              {
                                                  return sessmgr_event;
                                              });
            if (!res)
            {
                throw SessionManager::Proxy::Exception("New tunnel did not respond");
            }

            return Session::Create(proxy, session_path);
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("Failed to start new tunnel");
        }
    }


    const DBus::Object::Path::List FetchAvailableSessionPaths() const
    {
        try
        {
            GVariant *r = proxy->Call(target,
                                      "FetchAvailableSessions");
            auto sessions_list = glib2::Value::ExtractVector<DBus::Object::Path>(r, 0);
            return sessions_list;
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("Failed to retrieve sessions paths");
        }
    }

    /**
     *  Retrieve an array of Session objects for all available
     *  sessions
     *
     * @return  Session::List (aka std::vector<SessionManager::Proxy::Session::Ptr>)
     */
    const Session::List FetchAvailableSessions() const
    {
        Session::List ret{};
        for (const auto &session_path : FetchAvailableSessionPaths())
        {
            ret.push_back(Session::Create(proxy, session_path));
        }
        return ret;
    }


    Session::Ptr Retrieve(const DBus::Object::Path &session_path) const
    {
        return Session::Create(proxy, session_path);
    }


    const std::vector<std::string> FetchManagedInterfaces() const
    {
        try
        {
            GVariant *r = proxy->Call(target,
                                      "FetchManagedInterfaces");
            auto intf_list = glib2::Value::ExtractVector<std::string>(r, 0);
            return intf_list;
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("Failed to retrieve managed interfaces");
        }
    }


    /**
     *  Lookup all sessions which where started with the given configuration
     *  profile name.
     *
     * @param cfgname  std::string containing the configuration name to
     *                 look up
     *
     * @return Returns a DBus::Object::Path::list with all session object
     *         paths which were started with the given configuration name.
     *         If no match is found, the std::vector will be empty.
     */
    const DBus::Object::Path::List LookupConfigName(const std::string &cfgname) const
    {
        try
        {
            GVariant *r = proxy->Call(target,
                                      "LookupConfigName",
                                      glib2::Value::CreateTupleWrapped(cfgname));
            auto sessions_list = glib2::Value::ExtractVector<DBus::Object::Path>(r, 0);
            return sessions_list;
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw SessionManager::Proxy::Exception("Failed to lookup configuration names");
        }
    }

    /**
     *  Lookup the session path for a specific interface name.
     *
     * @param interface  std::string containing the interface name
     *
     * @return  Returns a DBus::Object::Path containing the D-Bus path to
     *          the session this interface is attached to.  If not found,
     *          and exception is thrown.
     */
    const DBus::Object::Path LookupInterface(const std::string &interface) const
    {
        try
        {
            GVariant *r = proxy->Call(target,
                                      "LookupInterface",
                                      glib2::Value::CreateTupleWrapped(interface));
            auto session_path = glib2::Value::Extract<DBus::Object::Path>(r, 0);
            if (session_path.empty())
            {
                throw TunInterfaceException("Failed to lookup interface '"
                                            + interface + "'");
            }

            return session_path;
        }
        catch (const DBus::Proxy::Exception &)
        {
            throw TunInterfaceException("Failed to lookup interface '"
                                        + interface + "'");
        }
    }

  private:
    DBus::Connection::Ptr dbuscon = nullptr;
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr target = nullptr;

    Manager(DBus::Connection::Ptr conn)
        : dbuscon(conn),
          proxy(DBus::Proxy::Client::Create(conn,
                                            Constants::GenServiceName("sessions"))),
          target(DBus::Proxy::TargetPreset::Create(Constants::GenPath("sessions"),
                                                   Constants::GenInterface("sessions")))
    {
        auto prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
        prxqry->CheckServiceAvail(Constants::GenServiceName("sessions"));

        // Delay the return up to 750ms, to ensure we have a valid
        // Session Manager service object available
        auto prxchk = DBus::Proxy::Utils::Query::Create(proxy);
        for (uint8_t i = 5; i > 0; i--)
        {
            if (prxchk->CheckObjectExists(target->object_path,
                                          target->interface))
            {
                break;
            }
            usleep(150000);
        }
    }
};

} // namespace SessionManager::Proxy
