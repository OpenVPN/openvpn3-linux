//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   sessionmgr.hpp
 *
 * @brief  Service side implementation of the Session Manager
 *         (net.openvpn.v3.sessions).
 *
 *         The SessionManagerDBus class establishes the service and registers
 *         it on the D-Bus.  This service object owns a single
 *         SessionManagerObject which is the main service object.  This is
 *         used to establish SessionObjects, one for each VPN tunnel started.
 */


#ifndef OPENVPN3_DBUS_SESSIONMGR_HPP
#define OPENVPN3_DBUS_SESSIONMGR_HPP

#include <cstring>
#include <functional>
#include <ctime>
#include <memory>

#include <openvpn/common/likely.hpp>
#include <openvpn/log/logsimple.hpp>

#include "common/core-extensions.hpp"
#include "common/lookup.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/glibutils.hpp"
#include "dbus/path.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "log/proxy-log.hpp"
#include "client/statusevent.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "sessionmgr-exceptions.hpp"
#include "sessionmgr-events.hpp"

using namespace openvpn;

/**
 * Helper class to tackle signals sent by the session manager
 *
 * This mostly just wraps the LogSender class and predfines LogGroup to always
 * be SESSIONMGR.
 */
class SessionManagerSignals : public LogSender
{
public:
    SessionManagerSignals(GDBusConnection *conn, std::string object_path,
                          unsigned int log_level, LogWriter *logwr,
                          bool signal_broadcast)
            : LogSender(conn, LogGroup::SESSIONMGR,
                        OpenVPN3DBus_interf_sessions, object_path, logwr),
              signal_broadcast(signal_broadcast)
    {
        SetLogLevel(log_level);
        if (!signal_broadcast)
        {
            DBusConnectionCreds credsprx(conn);
            AddTargetBusName(credsprx.GetUniqueBusID(OpenVPN3DBus_name_log));
        }
    }

    virtual ~SessionManagerSignals() = default;


    bool GetSignalBroadcast()
    {
        return signal_broadcast;
    }


    /**
     *  Whenever a FATAL error happens, the process is expected to stop.
     *  The abort() call gets caught by the main loop, which then starts the
     *  proper shutdown process.
     *
     * @param msg  Message to sent to the log subscribers
     */
    void LogFATAL(std::string msg)
    {
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, msg);
        abort();
    }


    /**
     *  Sends log messages tagged as debug message
     *
     * @param msg  Message to log
     */
    void Debug(std::string msg)
    {
            LogSender::Debug(msg);
    }

    /**
     *  An extended debug log function, which adds D-Bus message details
     *  related to the debug message
     *
     * @param busname  D-Bus bus name triggering this log event
     * @param path     D-Bus path to the object triggering this log event
     * @param pid      PID of the message sender triggering this log event
     * @param msg      The log message itself
     */
    void Debug(std::string busname, std::string path, pid_t pid, std::string msg)
    {
            std::stringstream debug;
            debug << "pid=" << std::to_string(pid)
                  << ", busname=" << busname
                  << ", path=" << path
                  << ", message=" << msg;
            LogSender::Debug(debug.str());
    }

    /**
     *  Sends a StatusChange signal with a text message
     *
     * @param major  StatusMajor code of the status change
     * @param minor  StatusMinro code of the status change
     * @param msg    String containing a description of the reason for this
     *               status change
     */
    void StatusChange(const StatusMajor major, const StatusMinor minor, std::string msg)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, msg.c_str());
        Send("StatusChange", params);
    }

    /**
     *  A simpler StatusChange signal sender, without a text message
     *
     * @param major  StatusMajor code of the status change
     * @param minor  StatusMinro code of the status change
     */
    void StatusChange(const StatusMajor major, const StatusMinor minor)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, "");
        Send("StatusChange", params);
    }

private:
    bool signal_broadcast = true;
};



/**
 *  Handler for session StatusChange signals.  This essentially proxies
 *  StatusChange signals from a VPN client backend process to any front-end
 *  processes subscribed to these signals.
 */
class SessionStatusChange : public DBusSignalSubscription,
                            public DBusSignalProducer
{
public:
    /**
     *  Constructor preparing the proxy of StatusChange events
     *
     * @param conn               D-Bus connection to use
     * @param bus_name           D-Bus bus name to use for the signal
     *                           subscription.  This must be a unique bus
     *                           name and NOT a well-known bus name, otherwise
     *                           signals will not be proxied further.
     * @param interface          D-Bus interface to use for the signal
     *                           subscription
     * @param session_path       D-Bus path to the session to retrieve
     *                           StatusChange events from and proxy forward as
     */
    SessionStatusChange(GDBusConnection *conn,
                        std::string bus_name,
                        std::string interface,
                        std::string session_path)
        : DBusSignalSubscription(conn, bus_name, interface,
                                 OpenVPN3DBus_rootp_backends_session,
                                 "StatusChange"),
          DBusSignalProducer(conn, "", OpenVPN3DBus_interf_sessions,
                             session_path),
          backend_busname(bus_name),
          last_status()
    {
    }

    /**
     *  Callback function used by the D-Bus library whenever a signal we are
     *  subscribed to occurs.
     *
     * @param connection       D-Bus connection where the signal happened
     * @param sender_name      D-Bus bus name of the sender of the signal
     * @param object_path      D-Bus object path where the signal came from
     * @param interface_name   D-Bus interface of the signal
     * @param signal_name      The name of the signal which was sent
     * @param parameters       Pointer to a GVariant GLib2 object containing
     *                         the variables and values the signal carries
     */
    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters)
    {
        //
        // Check if this signal matches the session we're managing
        // Since the sender bus name is the unique bus name,
        // bus name, backend_busname must contain the unique bus
        // name as well of the backend this object belongs to.
        //
        if (sender != backend_busname)
        {
            // Sender did not match our requirements, ignore it
            return;
        }

        // Signals we proxy further, ignore the rest
        if (signal_name == "StatusChange")
        {
            ProxyStatus(parameters);
        }
    }


    void ProxyStatus(GVariant *status)
    {
        StatusEvent s(status);

        // If the last status received was CONNECTION:CONN_AUTH_FAILED,
        // preserve this status message
        if (!(StatusMajor::CONNECTION == last_status.major
            && StatusMinor::CONN_AUTH_FAILED == last_status.minor))
        {
            last_status = s;
        }

        // Proxy this mesage via DBusSignalProducer
        Send("StatusChange", status);
    }


    /**
     *  Retrieve the last status message processed
     *
     * @return  Returns a GVariant object containing the last signal sent
     */
    GVariant * GetLastStatusChange()
    {
        if( last_status.empty())
        {
            return NULL;  // Nothing have been logged, nothing to report
        }
        return last_status.GetGVariantTuple();
    }


    /**
     *  Compares the provided status with what our latest registered status
     *  is.
     *
     * @param status_chk  GVariant object containing a status dict
     * @return Returns True if the provided status is identical with the last
     *         reigstered status.
     */
    bool CompareStatus(GVariant *status_chk)
    {
        if ( last_status.empty() )
        {
            // No status logged, so it is not possible to compare it.
            // Return false, as uncomparable statuses means we need to handle
            // this situation in the caller.
            return false;
        }

        StatusEvent chk(status_chk);
        return last_status == chk;
    }

private:
    std::string backend_busname;
    StatusEvent last_status;
};


enum class DCOstatus : unsigned short {
    UNCHANGED,
    MODIFIED,
    LOCKED
};


class SessionLogProxy : public LogServiceProxy
{
public:
    using Ptr = std::shared_ptr<SessionLogProxy>;

    SessionLogProxy(GDBusConnection* dbc,
                    const std::string& target_,
                    const std::string& session_path)
        : LogServiceProxy(dbc), target(target_)
    {
        logproxy = ProxyLogEvents(target, session_path);
    }


    ~SessionLogProxy()
    {
        if (logproxy)
        {
            logproxy->Remove();
            logproxy.reset();
        }
    }

    const std::string GetLogProxyPath() const
    {
        return (logproxy ? logproxy->GetPath() : "");
    }


    void SetLogLevel(unsigned int loglvl) const
    {
        if (logproxy)
        {
            logproxy->SetLogLevel(loglvl);
        }
    }

private:
    std::string target = {};
    LogProxy::Ptr logproxy = nullptr;
};

using SessionLogProxyList = std::map<std::string, SessionLogProxy::Ptr>;


/**
 *  A SessionObject contains information about a specific VPN client tunnel.
 *  Each time a new tunnel is created and initiated via D-Bus, the contents
 *  of that D-Bus object is contained within a SessionObject.  The session
 *  manager is be responsible for maintaining the life cycle of these objects.
 */
class SessionObject : public DBusObject,
                      public DBusSignalSubscription,
                      public DBusCredentials,
                      public SessionManagerSignals
{
public:
    /**
     *  Constructor creating a new SessionObject
     *
     * @param dbuscon  D-Bus connection this object is tied to
     * @param owner    An uid reference of the owner of this object.  This is
     *                 typically the uid of the front-end user initating the
     *                 creation of a new tunnel session.
     * @param objpath  D-Bus object path of this object
     * @param cfg_path D-Bus object path of the VPN profile configuration this
     *                 session is tied to.
     * @param manager_log_level Default log level, used by the session manager
     * @param logwr    Pointer to LogWriter object; can be nullptr to
     *                 disablefile log.
     *
     */
    SessionObject(GDBusConnection *dbuscon,
                  std::function<void()> remove_callback,
                  uid_t owner,
                  std::string objpath, std::string cfg_path,
                  unsigned int manager_log_level, LogWriter *logwr,
                  bool signal_broadcast)
        : DBusObject(objpath),
          DBusSignalSubscription(dbuscon, "", OpenVPN3DBus_interf_backends, ""),
          DBusCredentials(dbuscon, owner),
          SessionManagerSignals(dbuscon, objpath, manager_log_level, logwr,
                                signal_broadcast),
          remove_callback(remove_callback),
          be_proxy(nullptr),
          restrict_log_access(true),
          session_created(std::time(nullptr)),
          config_path(cfg_path),
          config_name(""),
          sig_statuschg(nullptr),
          backend_token(""),
          backend_pid(0),
          be_conn(nullptr),
          registered(false),
          selfdestruct_complete(false)
    {
        // Only for the initialization of this object, use the manager's
        // log level.  Once the object is registered with a backend, it
        // will switch to the default session log level.
        SetLogLevel(manager_log_level);
        Subscribe("RegistrationRequest");

        // Register configuration the configuration object
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_sessions << "'>"
                          << "        <method name='Connect'/>"
                          << "        <method name='Pause'>"
                          << "            <arg type='s' name='reason' direction='in'/>"
                          << "        </method>"
                          << "        <method name='Resume'/>"
                          << "        <method name='Restart'/>"
                          << "        <method name='Disconnect'/>"
                          << "        <method name='Ready'/>"
                          << "        <method name='AccessGrant'>"
                          << "            <arg direction='in' type='u' name='uid'/>"
                          << "        </method>"
                          << "        <method name='AccessRevoke'>"
                          << "            <arg direction='in' type='u' name='uid'/>"
                          << "        </method>"
                          << "        <method name='LogForward'>"
                          << "            <arg direction='in' type='b' name='enable'/>"
                          << "        </method>"
                          << RequiresQueue::IntrospectionMethods("UserInputQueueGetTypeGroup",
                                                                 "UserInputQueueFetch",
                                                                 "UserInputQueueCheck",
                                                                 "UserInputProvide")
                          << "        <signal name='AttentionRequired'>"
                          << "            <arg type='u' name='type' direction='out'/>"
                          << "            <arg type='u' name='group' direction='out'/>"
                          << "            <arg type='s' name='message' direction='out'/>"
                          << "        </signal>"
                          << GetStatusChangeIntrospection()
                          << GetLogIntrospection()
                          << "        <property type='u' name='owner' access='read'/>"
                          << "        <property type='t' name='session_created' access='read'/>"
                          << "        <property type='au' name='acl' access='read'/>"
                          << "        <property type='b' name='public_access' access='readwrite'/>"
                          << "        <property type='(uus)' name='status' access='read'/>"
                          << "        <property type='a{sv}' name='last_log' access='read'/>"
                          << "        <property type='a{sx}' name='statistics' access='read'/>"
                          << "        <property type='b' name='dco' access='readwrite'/>"
                          << "        <property type='s' name='device_path' access='read'/>"
                          << "        <property type='s' name='device_name' access='read'/>"
                          << "        <property type='o' name='config_path' access='read'/>"
                          << "        <property type='s' name='config_name' access='read'/>"
                          << "        <property type='s' name='session_name' access='read'/>"
                          << "        <property type='u' name='backend_pid' access='read'/>"
                          << "        <property type='b' name='restrict_log_access' access='readwrite'/>"
                          << "        <property type='ao' name='log_forwards' access='read'/>"
                          << "        <property type='u' name='log_verbosity' access='readwrite'/>"
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        try
        {
                // Start a new backend process via the openvpn3-service-backendstart
                // (net.openvpn.v3.backends) service.  A random backend token is
                // created and sent to the backend process.  When the backend process
                // have initialized, it reports back to the session manager using
                // this token as a reference.  This is used to tie the backend process
                // to this specific SessionObject.
                backend_token = generate_path_uuid("", 't');

                auto backend_start = DBusProxy(G_BUS_TYPE_SYSTEM,
                                               OpenVPN3DBus_name_backends,
                                               OpenVPN3DBus_interf_backends,
                                               OpenVPN3DBus_rootp_backends);
                backend_start.Ping(); // Wake up the backend service first
                (void) backend_start.GetServiceVersion();

                GVariant *res_g = backend_start.Call("StartClient",
                                                      g_variant_new("(s)", backend_token.c_str()));
                if (NULL == res_g) {
                        THROW_DBUSEXCEPTION("SessionObject",
                                            "Failed to extract the result of the "
                                            "StartClient request");
                }
                g_variant_get(res_g, "(u)", &backend_pid);
        }
        catch (DBusException& excp)
        {
            StatusChange(StatusMajor::SESSION, StatusMinor::PROC_STOPPED);
            LogCritical(excp.what());
            return;
        }

        // The PID value we get here is just a temporary.  This is the
        // PID returned by openvpn3-service-backendstart.  This will again
        // start the openvpn3-service-client process, which will fork() once
        // to be completely independent.  When this last fork() happens,
        // the backend will report back its final PID.
        StatusChange(StatusMajor::SESSION, StatusMinor::PROC_STARTED,
                             "session_path=" + DBusObject::GetObjectPath()
                             + ", backend_pid=" + std::to_string(backend_pid));
        Debug("SessionObject registered on '" + OpenVPN3DBus_interf_sessions + "': "
              + objpath + " [backend_token=" + backend_token + "]");

        std::stringstream msg;
        msg << "Session starting, configuration path: " << cfg_path
            << ", owner: " << lookup_username(owner);
        LogVerb1(msg.str());
    }

    ~SessionObject()
    {
        if (sig_statuschg)
        {
            delete sig_statuschg;
        }

        if (be_proxy)
        {
            delete be_proxy;
        }
        LogVerb1("Session is closing");
        StatusChange(StatusMajor::SESSION, StatusMinor::SESS_REMOVED);
        remove_callback();
        IdleCheck_RefDec();
    }

    /**
     *  Retrieve the initial configuration profile name used for this session.
     *  If the profile name is changed after this session started, the
     *  config name returned will still be the old name.
     *
     * @return Returns std::string containing the configuration profile name.
     */
    std::string GetConfigName() const noexcept
    {
        return config_name;
    }


    /**
     *  Retrieve the device name used by the this session.
     *
     * @return Returns std::string containing the device string.  If the
     *         connection to the backend process is not established, an
     *         empty string is returned.
     */
    std::string GetDeviceName() const
    {
        if (!be_proxy)
        {
            return "";
        }
        try
        {
            return be_proxy->GetStringProperty("device_name");
        }
        catch (const DBusException& excp)
        {
            // Ignore errors, just report an empty device name
            return "";
        }
    }


    /**
     *  Callback method called each time signals we have subscribed to
     *  occurs.  For the SessionObject, we care about these signals:
     *
     *    - RegistrationRequest:  which the VPN client backend process
     *                            sends once it has completed the
     *                            initialization.
     *    - StatusChange:         whenever the status changes in the backend
     *    - AttentionRequired:    whenever the backend process needs
     *                            information from the front-end user.
     *
     * @param conn             D-Bus connection where the signal came from
     * @param sender_name      D-Bus bus name of the sender of the singal
     * @param object_path      D-Bus object path of the signal
     * @param interface_name   D-Bus interface of the signal
     * @param signal_name      The name of the signal itself
     * @param params           GVariant Glib2 object containing all the
     *                         variables the signal carries
     */
    void callback_signal_handler(GDBusConnection *conn,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *params)
    {
        if ((signal_name == "RegistrationRequest")
            && (interface_name == OpenVPN3DBus_interf_backends))
        {
            gchar *busn = nullptr;
            gchar *sesstoken_c = nullptr;
            pid_t be_pid;
            g_variant_get (params, "(ssi)", &busn, &sesstoken_c, &be_pid);

            be_conn = conn;
            be_busname = std::string(busn);

            g_free(busn);
            std::string sesstoken(sesstoken_c);
            g_free(sesstoken_c);
            be_path = std::string(object_path);

            if (std::string(sesstoken) != backend_token)
            {
                // This registration request was not for us.
                // The RegistrationRequest signals is broadcasted which means
                // any SessionObject still subscribed to this signal will
                // see this request.
                //
                // This can happen if several tunnels are started in parallel
                // and they issue this signal at approximately the same time
                // and before the owning SessionObject() have managed to
                // unsubscribe from the signal

                // Debug("Ignoring RegistrationRequest (token mismatch)  - name=" + be_busname + ", path=" + be_path);
                return;
            }

            try
            {
                Subscribe(sender_name, be_path, "AttentionRequired");
                Subscribe(sender_name, be_path, "StatusChange");
                register_backend();
                backend_pid = be_pid;
                Unsubscribe("RegistrationRequest");
                SetLogLevel(default_session_log_level);
                LogVerb2("Backend VPN client process registered");
            }
            catch (DBusException& err)
            {
                LogError("Could not register backend process, removing session object");
                Debug(be_busname, be_path, backend_pid, std::string(err.what()));
                StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, "Backend process died");
                selfdestruct(conn);
            }
        }
        else if ((signal_name == "StatusChange")
                 && (interface_name == OpenVPN3DBus_interf_backends))
        {
            StatusEvent status(params);

            if (StatusMajor::CONNECTION == status.major
                && StatusMinor::CONN_FAILED == status.minor)
            {
                // When the backend client signals connection failure
                // force it to shutdown and close this session object
                //
                // FIXME: Consider if we need to split CONN_FAILED into
                //        a fatal error (as now) and "Connection failed, but session may resume"
                //        This link is between here and client/core-client.hpp:173
                //
                shutdown(true, (StatusMinor::CONN_FAILED == status.minor));
            }
        }
        else if ((signal_name =="AttentionRequired")
                 && (interface_name == OpenVPN3DBus_interf_backends))
        {
                // Proxy this signal directly to the front-end processes
                // listening
                Send("AttentionRequired", params);
        }
    }

    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this SessionObject.
     *
     *  In most cases the method call is just proxied to the client backend
     *  process after an access control check has been performed.  Many of the
     *  calls can be accessed by the owner or designated user IDs, and the
     *  most sensitive methods are only accessible to the owner of this
     *  session.
     *
     * @param conn       D-Bus connection where the method call occurred
     * @param sender     D-Bus bus name of the sender of the method call
     * @param obj_path   D-Bus object path of the target object.
     * @param intf_name  D-Bus interface of the method call
     * @param method_name D-Bus method name to be executed
     * @param params     GVariant Glib2 object containing the arguments for
     *                   the method call
     * @param invoc      GDBusMethodInvocation where the response/result of
     *                   the method call will be returned.
     */
    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        bool ping = false;

        try
        {
            if (!be_proxy)
            {
                THROW_DBUSEXCEPTION("SessionObject", "No backend proxy connection available. Backend died?");
            }
            try {
                ping = ping_backend();
                if (unlikely(!ping))
                {
                    THROW_DBUSEXCEPTION("SessionObject",
                                        "The response from the backend Ping request was surprising");
                }
            }
            catch (DBusException &dbserr)
            {
                ping = false;
                THROW_DBUSEXCEPTION("SessionObject",
                                    "Backend did not respond: "
                                    + std::string(dbserr.GetRawError()));
            }

            if (!registered)
            {
                THROW_DBUSEXCEPTION("SessionObject",
                                    "Session registration not completed");
            }

            std::stringstream msg;
            msg << "Session operation: " << method_name
                << ", requester:  " << lookup_username(GetUID(sender));
            Debug(msg.str());

            if ("Connect" == method_name)
            {
                CheckACL(sender);

                if (DCOstatus::MODIFIED == dco_status)
                {
                    be_proxy->SetProperty("dco", dco);
                    dco_status = DCOstatus::LOCKED;
                }
                try
                {
                    auto verb = (unsigned int)be_proxy->GetUIntProperty("log_level");
                    SetLogLevel(verb);
                }
                catch (const DBusException&)
                {
                    LogCritical("Could not retrieve the client backend log level");
                }
                be_proxy->Call("Connect");
                LogVerb2("Starting connection");
            }
            else if ("Restart" == method_name)
            {
                CheckACL(sender, true);
                be_proxy->Call("Restart");
                LogVerb2("Restarting connection");
            }
            else if ("Pause" == method_name)
            {
                CheckACL(sender, true);
                // FIXME: Should check that params contains only the expected formatting
                be_proxy->Call("Pause", params);
                LogVerb2("Pausing connection");
            }
            else if ("Resume"  == method_name)
            {
                CheckACL(sender, true);
                be_proxy->Call("Resume");
                LogVerb2("Resuming connection");
            }
            else if ("Disconnect" == method_name)
            {
                CheckACL(sender, true);
                LogVerb2("Disconnecting connection");
                shutdown(false, true);
            }
            else if ("Ready" == method_name)
            {
                try
                {
                    CheckACL(sender);
                    be_proxy->Call("Ready");
                }
                catch (DBusException& dberr)
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.sessions.error",
                                                                  dberr.what());
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }
            }
            else if ("UserInputQueueGetTypeGroup" == method_name)
            {
                CheckACL(sender);
                try
                {
                    GVariant *res = be_proxy->Call("UserInputQueueGetTypeGroup", params);
                    g_dbus_method_invocation_return_value(invoc, res);
                    g_variant_unref(res);
                }
                catch (RequiresQueueException& excp)
                {
                    excp.GenerateDBusError(invoc);
                }
                return;
            }
            else if ("UserInputQueueFetch" == method_name)
            {
                CheckACL(sender);
                try
                {
                    GVariant *res = be_proxy->Call("UserInputQueueFetch", params);
                    g_dbus_method_invocation_return_value(invoc, res);
                    g_variant_unref(res);
                }
                catch (RequiresQueueException& excp)
                {
                    excp.GenerateDBusError(invoc);
                }
                return;
            }
            else if ("UserInputQueueCheck" == method_name)
            {
                CheckACL(sender);
                GVariant *res = be_proxy->Call("UserInputQueueCheck", params);
                g_dbus_method_invocation_return_value(invoc, res);
                g_variant_unref(res);
                return;
            }
            else if ("UserInputProvide" == method_name)
            {
                CheckACL(sender);
                try
                {
                    GVariant *res = be_proxy->Call("UserInputProvide", params);
                    g_dbus_method_invocation_return_value(invoc, res);
                    g_variant_unref(res);
                }
                catch (RequiresQueueException& excp)
                {
                    // Convert this exception into an error sent back
                    // to the requester as a D-Bus error instead.
                    excp.GenerateDBusError(invoc);
                }
                return;
            }
            else if ("AccessGrant" == method_name)
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                GrantAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogInfo("Access granted to UID " + std::to_string(uid));
                return;
            }
            else if ("AccessRevoke" == method_name)
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                RevokeAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogInfo("Access revoked for UID " + std::to_string(uid));
                return;
            }
            else if ("LogForward" == method_name)
            {
                if (restrict_log_access)
                {
                    CheckOwnerAccess(sender);
                }
                else
                {
                    CheckACL(sender);
                }

                GLibUtils::checkParams(__func__, params, "(b)", 1);
                bool enable = GLibUtils::ExtractValue<bool>(params, 0);
                if (enable)
                {
                    log_proxies[sender].reset(
                            new SessionLogProxy(DBusSignalSubscription::GetConnection(),
                                                sender,
                                                DBusObject::GetObjectPath()));
                    LogInfo("Added log forwarding to " + sender);
                }
                else
                {
                    try
                    {
                        log_proxies.at(sender).reset();
                        log_proxies.erase(sender);
                    }
                    catch(const std::exception& e)
                    {
                        LogCritical("logproxies.erase(" + sender + ") failed:" + std::string(e.what()));
                    }
                    LogInfo("Removed log forwarding from " + sender);
                }
                g_dbus_method_invocation_return_value(invoc, NULL);
                return;
            }
            else
            {
                std::string errmsg = "No method named" + method_name + " is available";
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.sessions.error",
                                                              errmsg.c_str());
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                LogWarn(errmsg);
                return;
            }
            g_dbus_method_invocation_return_value(invoc, NULL);
        }
        catch (DBusException& dberr)
        {
            bool do_selfdestruct = false;
            std::string errmsg;

            Debug("Exception [callback_method_call("+ method_name + ")]: "
                  + dberr.GetRawError());

            if (!registered && "Disconnect" == method_name)
            {
                //
                // This is a special case handling.  If a backend VPN client
                // process has not registered and a front-end wants to
                // shutdown this session, this needs to be handled specially.
                //
                // This can happen if the VPN client process dies before
                // starting the registration process.  In this case, we
                // will not have a valid be_proxy object to the backend,
                // so any kind of attempt to communicate with the (possibly
                // dead) backend will just completely fail.  So we just aim
                // for the selfdestruct.
                //
                // In regards to the front-end caller, this is also not
                // strictly an error. The caller wants to get rid of this
                // lingering session object by calling the Disconnect method.
                // So we do not return any error, but just ensure this
                // session is cleaned up properly.
                //
                try
                {
                    LogCritical("Forced session shutdown before backend registration");
                    do_selfdestruct = true;
                }
                catch (DBusException& dberr)
                {
                }
                StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED,
                             "Backend process did not complete registration");
                g_dbus_method_invocation_return_value(invoc, NULL);
            }
            else if (!registered)
            {
                errmsg = "Backend VPN process is not ready";
            }
            else if (!ping)
            {
                try
                {
                    shutdown(true, true); // Ensure the backend client process have stopped
                }
                catch (DBusException& dberr)
                {
                    // Ignore any errors for now.
                }
                errmsg = "Backend VPN process has died.  Session is no longer valid.";
                if (!selfdestruct_complete)
                {
                    StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, "Backend process died");
                    do_selfdestruct = true;
                }
            }
            else
            {
                errmsg = "Failed communicating with VPN backend: "
                       + std::string(dberr.GetRawError());
            }

            if (registered && !selfdestruct_complete && !do_selfdestruct)
            {
                LogCritical(errmsg);
            }

            if (!errmsg.empty())
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.sessions.error",
                                                          errmsg.c_str());
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
            }

            if (do_selfdestruct)
            {
                selfdestruct(conn);
            }
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.what());
            excp.SetDBusError(invoc);
        }
    };


    /**
     *   Callback which is used each time a SessionObject D-Bus property is
     *   being read.
     *
     *   Only the 'owner' is accessible by anyone, otherwise it must either
     *   be the session owner or UIDs granted access to this session.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns a GVariant Glib2 object containing the value of the
     *          requested D-Bus object property.  On errors, NULL must be
     *          returned and the error must be returned via a GError
     *          object.
     */
    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        if (!registered)
        {
            g_set_error(error,
                        G_IO_ERROR,
                        G_IO_ERROR_PENDING,
                        "Session not active");
            return NULL;
        }

        try
        {
            CheckACL_allowRoot(sender);
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.what());
            excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_FAILED);
            return NULL;
        }
        catch (const DBusException& excp)
        {
            std::string err(excp.what());

            if (err.find("Could not get UID of name") != std::string::npos)
            {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Backend service unavailable");
            }
            else
            {
                LogCritical(excp.what());
                excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE);
            }
            return NULL;
        }


        GVariant *ret = NULL;
        if ("owner" == property_name)
        {
            ret = GetOwner();
        }
        else if ("device_path" == property_name)
        {
            try
            {
                ret = be_proxy->GetProperty("device_path");
            }
            catch (DBusException&)
            {
                g_set_error(error,
                            G_IO_ERROR,
                            G_IO_ERROR_PENDING,
                            "Device path not available");
                return NULL;
            }
        }
        else if ("restrict_log_access" == property_name)
        {
            ret = g_variant_new_boolean (restrict_log_access);
        }
        else if ("last_log" == property_name)
        {
            try
            {
                ret = be_proxy->GetProperty("last_log_line");
            }
            catch (DBusException&)
            {
                ret = GLibUtils::CreateEmptyBuilderFromType("a{sv}");
            }
        }
        else if ("session_created" == property_name)
        {
            ret = g_variant_new_uint64(session_created);
        }
        else if ("status" == property_name)
        {
            try
            {
                if (!be_proxy->CheckObjectExists())
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT,
                                "Backend object not available");
                    return NULL;
                }

                ret = NULL;
                if (nullptr != sig_statuschg)
                {
                    update_last_status();
                    ret = sig_statuschg->GetLastStatusChange();
                }
                if (NULL == ret)
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY,
                                "No status changes have been logged yet");
                }
            }
            catch (DBusException& excp)
            {
                g_set_error(error, G_DBUS_ERROR, G_IO_ERROR_FAILED,
                            "Failed retrieving connection status");
                ret = NULL;
            }
        }
        else if ("statistics" == property_name)
        {
            try
            {
                if (!be_proxy->CheckObjectExists())
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT,
                                "Backend object not available");
                    return NULL;
                }
                ret = be_proxy->GetProperty("statistics");
            }
            catch (DBusException& exp)
            {
                g_set_error(error, G_DBUS_ERROR, G_IO_ERROR_FAILED,
                            "Failed retrieving connection statistics");
                ret = NULL;
            }
        }
        else if ("device_name" == property_name)
        {
            try
            {
                ret = be_proxy->GetProperty("device_name");
            }
            catch (DBusException&)
            {
                g_set_error(error,
                            G_IO_ERROR,
                            G_IO_ERROR_PENDING,
                            "Device path not available");
                return NULL;
            }
        }
        else if ("config_path" == property_name)
        {
            ret = g_variant_new_string (config_path.c_str());
        }
        else if ("config_name" == property_name)
        {
            ret = g_variant_new_string (config_name.c_str());
        }
        else if ("dco" == property_name)
        {
            try
            {
                ret = be_proxy->GetProperty("dco");
            }
            catch (DBusException&)
            {
                // If the DCO flag of the backend process could not be
                // retrieved, return our current setting
                ret = g_variant_new_boolean(dco);
            }
        }
        else if ("session_name" == property_name)
        {
            try
            {
                std::string sn(be_proxy->GetStringProperty("session_name"));
                ret = g_variant_new_string (sn.c_str());
            }
            catch (const DBusException& excp)
            {
                // Ignore errors in this case; this is informal details
                return g_variant_new_string("");
            }
        }
        else if ("backend_pid" == property_name)
        {
            ret = g_variant_new_uint32 (backend_pid);
        }
        else if ("log_forwards" == property_name)
        {
            std::vector<std::string> paths = {};
            for (const auto& it : log_proxies)
            {
                paths.push_back(it.second->GetLogProxyPath());
            }
            ret = GLibUtils::GVariantFromVector(paths);
        }
        else if ("log_verbosity" == property_name)
        {
            ret = g_variant_new_uint32 (GetLogLevel());
        }
        else if ("public_access" == property_name)
        {
            ret = g_variant_new_boolean(GetPublicAccess());
        }
        else if ("acl" == property_name)
        {
            ret = GLibUtils::GVariantFromVector(GetAccessList());
        }
        else
        {
            g_set_error(error,
                        G_IO_ERROR,
                        G_IO_ERROR_FAILED,
                        "Unknown property");
        }

        return ret;
    };


    /**
     *  Callback method which is used each time a SessionObject property
     *  is being modified over the D-Bus.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Returns a GVariantBuilder object which will be used by the
     *         D-Bus library to issue the required PropertiesChanged signal.
     *         If an error occurres, the DBusPropertyException is thrown.
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        /*
          std::cout << "[SessionObject] set_property(): "
                  << "sender=" << sender
                  << ", object_path=" << obj_path
                  << ", interface=" << intf_name
                  << ", property=" << property_name
                  << std::endl;
        */
        if (!registered)
        {
            g_set_error(error,
                        G_IO_ERROR,
                        G_IO_ERROR_PENDING,
                        "Session not active");
            return NULL;
        }

        try
        {
            if (!restrict_log_access
                && "log_verbosity" == property_name)
            {
                CheckACL(sender);
            }
            else
            {
                CheckOwnerAccess(sender);
            }
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.what());
            Debug("SetProperty - Object path: " + obj_path
                  + ", interface=" + intf_name
                  + ", property_name=" + property_name
                  + ", sender=" + sender);
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
        }

        try
        {
            if (("dco" == property_name) && be_conn)
            {
                if (DCOstatus::LOCKED != dco_status)
                {
                    dco = g_variant_get_boolean(value);
                    dco_status = DCOstatus::MODIFIED;
                    return build_set_property_response(property_name, dco);
                }
                else
                {
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                obj_path, intf_name,
                                                property_name,
                                                "DCO setting cannot be changed now");
                }
            }
            else if (("restrict_log_access" == property_name) && be_conn)
            {
                restrict_log_access = g_variant_get_boolean(value);
                return build_set_property_response(property_name,
                                                   restrict_log_access);
            }
            else if (("log_verbosity" == property_name) && be_conn)
            {
                unsigned int log_verb = g_variant_get_uint32(value);
                try
                {
                    SetLogLevel(log_verb);
                }
                catch (const LogException& excp)
                {
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                obj_path, intf_name,
                                                property_name,
                                                excp.what());
                }

                try
                {
                    if (be_proxy)
                    {
                        be_proxy->SetProperty("log_level", log_verb);
                    }
                }
                catch (const DBusException& e)
                {
                    LogCritical("Could not set the VPN backend log level");
                    Debug("Backend log-level error: "
                          + std::string(e.GetRawError()));
                }
                return build_set_property_response(property_name,
                                                   (guint32) log_verb);
            }
            else if (("public_access" == property_name) && conn)
            {
                bool acl_public = g_variant_get_boolean(value);
                SetPublicAccess(acl_public);
                LogInfo("Public access set to "
                         + (acl_public ? std::string("true") :
                                         std::string("false"))
                         + " by uid " + std::to_string(GetUID(sender)));
                return build_set_property_response(property_name, acl_public);
            }
        }
        catch (DBusException& excp)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.what());
        }
        throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                    obj_path, intf_name, property_name,
                                    "Invalid property");
    }


    /**
     *  Clean-up function triggered by the D-Bus library when an object
     *  is removed from the D-Bus
     */
    void callback_destructor ()
    {
        if( nullptr != sig_statuschg)
        {
            delete sig_statuschg;
            sig_statuschg = nullptr;
        }
    };


private:
    unsigned int default_session_log_level = 4; // LogCategory::INFO messages
    std::function<void()> remove_callback;
    DBusProxy *be_proxy;
    bool restrict_log_access;
    SessionLogProxyList log_proxies= {};
    std::time_t session_created;
    std::string config_path;
    std::string config_name;
    bool dco = false;
    DCOstatus dco_status = DCOstatus::UNCHANGED;
    SessionStatusChange *sig_statuschg;
    std::string backend_token;
    pid_t backend_pid;
    GDBusConnection *be_conn;
    std::string be_busname;
    std::string be_path;
    bool registered;
    bool selfdestruct_complete;
    std::mutex selfdestruct_guard;


    /**
     *  Ties the VPN client backend process to this SessionObject.  Once that
     *  is done, it calls the RegistrationConfirmation method in the backend
     *  process where it confirms the backend token and provides the
     *  VPN configuration D-Bus object path to the backend.
     */
    void register_backend()
    {
        try
        {
            be_proxy = new DBusProxy(G_BUS_TYPE_SYSTEM,
                                     be_busname,
                                     OpenVPN3DBus_interf_backends,
                                     be_path);
            // Don't try to auto start backend services over D-Bus,
            // The backend service should exists _before_ we try to
            // communicate with it.
            be_proxy->SetGDBusCallFlags(G_DBUS_CALL_FLAGS_NO_AUTO_START);

            unsigned int attempts = 5;
            while (attempts > 0)
            {
                try
                {
                    ping_backend();
                    attempts = 0;
                }
                catch (const DBusException& excp)
                {
                    --attempts;
                    if (attempts > 0)
                    {
                        // If we didn't manage to get a Ping response
                        // it might be the backend client process is a bit
                        // slow to settle.  So we just wait for another
                        // second and try again
                        sleep(1);
                    }
                    else
                    {
                        // This didn't work, so bail out.
                        throw;
                    }
                }
            }

            // Setup signal listeners from the backend process
            // The SessionStatusChange() handler will use the senders
            // unique bus name to identify if this is a signal this class
            // responsible for.
            //
            // As the be_busname contains the well-known bus name of the
            // backend VPN client process, we resolve this to the unique
            // bus name for this signal handling object.
            sig_statuschg = new SessionStatusChange(be_conn,
                                                    GetUniqueBusID(be_busname),
                                                    OpenVPN3DBus_interf_backends,
                                                    DBusObject::GetObjectPath());

            // Retrieve the ACL and ownership transfer information from configmgr
            auto cfgprx = OpenVPN3ConfigurationProxy(G_BUS_TYPE_SYSTEM,
                                                         config_path);
            if (cfgprx.GetTransferOwnerSession())
            {
                uid_t curr_owner = GetOwnerUID();
                TransferOwnership(cfgprx.GetOwner());
                // This is needed for the session starter to be allowed to
                // complete the connecting phase
                GrantAccess(curr_owner);
                restrict_log_access = false;
            }

            GVariant *res_g = be_proxy->Call("RegistrationConfirmation",
                                             g_variant_new("(soo)",
                                                           backend_token.c_str(),
                                                           DBusObject::GetObjectPath().c_str(),
                                                           config_path.c_str()));
            if (nullptr == res_g)
            {
                THROW_DBUSEXCEPTION("SessionObject",
                                    "Failed to extract the result of the "
                                    "RegistrationConfirmation response");
            }

            gchar *cfgname_c = nullptr;
            g_variant_get(res_g, "(s)", &cfgname_c);
            if (!cfgname_c)
            {
                // FIXME: Find a way to gracefully handle failed registration
                return;
            }
            config_name = std::string(cfgname_c);
            Debug("New session registered: " + DBusObject::GetObjectPath());
            StatusChange(StatusMajor::SESSION, StatusMinor::SESS_NEW,
                         "session_path=" + DBusObject::GetObjectPath()
                         + " backend_busname=" + be_busname
                         + " backend_path=" + be_path);
            registered = true;
        }
        catch (DBusException& err)
        {
            THROW_DBUSEXCEPTION("SessionObject", err.what());
        }
    }


    /**
     * Simple ping-pong game between this SessionObject and its VPN client
     * backend.  If the backend does not respond, we treat it as dead and will
     * start to clean-up this session.
     *
     * @return  Returns True if the backend process is alive, otherwise False.
     */
    bool ping_backend()
    {
        if (nullptr == be_proxy)
        {
            THROW_DBUSEXCEPTION("SessionObject",
                                "No backend proxy connection established, "
                                "cannot ping backend");
        }

        GVariant *res_g = NULL;
        try {
            // This Ping() is the BackendClientObject responding,
            // which ensures the VPN client process is initialized
            res_g = be_proxy->Call("Ping");
        }
        catch (DBusException &dbserr)
        {
            res_g = NULL;
            Debug(be_busname, be_path, backend_pid, std::string(dbserr.what()));
        }
        if (NULL == res_g)
        {
            THROW_DBUSEXCEPTION("SessionObject",
                                "VPN backend process unavailable, "
                                "does not respond to internal Ping()");
        }

        bool ret = false;
        g_variant_get(res_g, "(b)", &ret);
        g_variant_unref(res_g);
        return ret;
    }


    /**
     * Fetches the last backend status and compares to what we have
     * registered.  If there is a mismatch, we might have missed a signal -
     * so register it and send it again.
     */
    void update_last_status()
    {
        if (!sig_statuschg)
        {
            return;
        }

        try
        {
            GVariant *be_status = nullptr;
            be_status = be_proxy->GetProperty("status");
            if (!sig_statuschg->CompareStatus(be_status))
            {
                sig_statuschg->ProxyStatus(be_status);
            }
        }
        catch (DBusException& excp)
        {
            // Ignore these failures for now.  It most commonly means
            // the backend process has closed/shut-down - this is scenario
            // is handled elsewhere in the call chain.

            // TODO: Consider to set the local status to PROC_KILLED or
            // something similar.
        }
    }


    /**
     *  Initiate a shutdown of the VPN client backend process.
     *
     * @param forced             If set to True, it will not do a normal
     *                           disconnect but tell the backend process
     *                           to stop more abruptly.
     * @param selfdestruct_flag  If set to True, this D-Bus session object
     *                           will be destroyed.  If not, it needs to
     *                           be removed later on independently.  Used to
     *                           allow front-ends to retrieve the last sent
     *                           status message, which can be AUTH_FAILED.
     */
    void shutdown(bool forced, bool selfdestruct_flag)
    {
        try
        {
            be_proxy->Call( (!forced ? "Disconnect" : "ForceShutdown"), true );
            // Wait for child to exit
            sleep(2); // FIXME: Catch the ProcessChange StatusMinor::PROC_STOPPED signal from backend
        }
        catch (DBusException& excp)
        {
            Debug(excp.what());
            // FIXME: For now, we just ignore any errors here - the
            // backend process may not be running
        }

        // Remove this session object
        if (!forced)
        {
            StatusChange(StatusMajor::SESSION, StatusMinor::PROC_STOPPED, "Session closed");
        }
        else
        {
            StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, "Session closed, killed backend client");
        }

        if (selfdestruct_flag)
        {
            selfdestruct(DBusSignalSubscription::GetConnection());
        }
    }


    /**
     *  This method is dangerous and should only be used by either the
     *  SessionObject::shutdown() method or exception handlers in the
     *  SessionObject.
     *
     *  This will initiate deleting this SessionObject from the D-Bus and then
     *  destroy itself.
     *
     * @param conn  D-Bus connection to use when removing this object from
     *              the D-Bus.
     */
    void selfdestruct(GDBusConnection *conn)
    {
        // Object may still be available via other threads for a short
        // while, which can cause havoc if not handled properly.  There
        // are a few call-chains when the backend client process dies which
        // may call selfdestruct() a couple of times at the almost the same
        // time.  So we add a lock here, to ensure only the first
        // selfdestruct() event is handled.  After this first call have
        // completed, this object is to be considered dead.
        //
        // !! WARNING: ONLY EXCEPTION HANDLERS AND shutdown()  !!
        // !! WARNING:       MAY CALL THIS FUNCTION!           !!
        //
        std::lock_guard<std::mutex> guard(selfdestruct_guard);
        if (selfdestruct_complete)
        {
            return;
        }
        RemoveObject(conn);
        selfdestruct_complete = true;

        delete this;
    }
};


/**
 *   A SessionManagerObject is the main entry point when starting new
 *   VPN tunnels.  It should only exist a single SessionManagerObject during
 *   the life cycle of the openvpn3-service-sessoinmgr process.
 */
class SessionManagerObject : public DBusObject,
                             public SessionManagerSignals,
                             public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<SessionManagerObject> Ptr;

    /**
     *  Constructor initializing the SessionManagerObject to be registered on
     *  the D-Bus.
     *
     * @param dbuscon  D-Bus this object is tied to
     * @param objpath  D-Bus object path to this object
     * @param logwr    Pointer to LogWriter object; can be nullptr to
     *                 disablefile log.
     *
     */
    SessionManagerObject(GDBusConnection *dbuscon, const std::string objpath,
                         unsigned int manager_log_level, LogWriter *logwr,
                         bool signal_broadcast)
        : DBusObject(objpath),
          SessionManagerSignals(dbuscon, objpath, manager_log_level, logwr,
                                signal_broadcast),
          dbuscon(dbuscon),
          creds(dbuscon)
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_sessions << "'>"
                          << "        <method name='NewTunnel'>"
                          << "          <arg type='o' name='config_path' direction='in'/>"
                          << "          <arg type='o' name='session_path' direction='out'/>"
                          << "        </method>"
                          << "        <method name='FetchAvailableSessions'>"
                          << "          <arg type='ao' name='paths' direction='out'/>"
                          << "        </method>"
                          << "        <method name='FetchManagedInterfaces'>"
                          << "          <arg type='as' name='devices' direction='out'/>"
                          << "        </method>"
                          << "        <method name='LookupConfigName'>"
                          << "           <arg type='s' name='config_name' direction='in'/>"
                          << "           <arg type='ao' name='session_paths' direction='out'/>"
                          << "        </method>"
                          << "        <method name='LookupInterface'>"
                          << "           <arg type='s' name='device_name' direction='in'/>"
                          << "           <arg type='o' name='session_path' direction='out'/>"
                          << "        </method>"
                          << "        <method name='TransferOwnership'>"
                          << "           <arg type='o' name='path' direction='in'/>"
                          << "           <arg type='u' name='new_owner_uid' direction='in'/>"
                          << "        </method>"
                          << "        <property type='s' name='version' access='read'/>"
                          << GetLogIntrospection()
                          << SessionManager::Event::GetIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        Debug("SessionManagerObject registered on '" + OpenVPN3DBus_interf_sessions + "': "
                      + objpath);
    }

    ~SessionManagerObject()
    {
        LogInfo("Shutting down");
        RemoveObject(dbuscon);
    }


    /**
     *  Callback method called each time a method in the SessionManagerObject
     *  is called over the D-Bus.
     *
     * @param conn       D-Bus connection where the method call occurred
     * @param sender     D-Bus bus name of the sender of the method call
     * @param obj_path   D-Bus object path of the target object.
     * @param intf_name  D-Bus interface of the method call
     * @param method_name D-Bus method name to be executed
     * @param params     GVariant Glib2 object containing the arguments for
     *                   the method call
     * @param invoc      GDBusMethodInvocation where the response/result of
     *                   the method call will be returned.
     */
    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        // std::cout << "SessionManagerObject::callback_method_call: " << method_name << std::endl;
        if ("NewTunnel" == method_name)
        {
            IdleCheck_UpdateTimestamp();

            // Retrieve the configuration path for the tunnel
            // from the request
            gchar *config_path_s = nullptr;
            g_variant_get (params, "(o)", &config_path_s);
            auto config_path = std::string(config_path_s);
            g_free(config_path_s);

            // Create session object, which will proxy calls
            // from the front-end to the backend
            std::string sesspath = generate_path_uuid(OpenVPN3DBus_rootp_sessions, 's');

            // Create the new object and register it in D-Bus
            auto callback = [self=Ptr(this), sesspath]()
                            {
                                self->remove_session_object(sesspath);
                            };
            SessionObject *session = new SessionObject(conn,
                                                       callback,
                                                       creds.GetUID(sender),
                                                       sesspath,
                                                       config_path,
                                                       GetLogLevel(),
                                                       logwr,
                                                       GetSignalBroadcast());
            IdleCheck_RefInc();
            session->IdleCheck_Register(IdleCheck_Get());
            session->RegisterObject(conn);
            session_objects[sesspath] = session;
            SessionManager::Event ev{sesspath,
                                     SessionManager::EventType::SESS_CREATED,
                                     creds.GetUID(sender)
                                     };
            Broadcast(OpenVPN3DBus_interf_sessions,
                      OpenVPN3DBus_rootp_sessions,
                      "SessionManagerEvent",
                      ev.GetGVariant());

            // Return the path to the new session object object to the caller
            // The backend object will remind "hidden" for the end-user
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(o)", sesspath.c_str()));
        }
        else if ("FetchAvailableSessions" == method_name
                || "FetchManagedInterfaces" == method_name)
        {
            bool ret_iface = ("FetchManagedInterfaces" == method_name);

            // Build up an array of object paths or device list of available
            // session objects
            GVariantBuilder *bld;
            bld = g_variant_builder_new(G_VARIANT_TYPE(ret_iface ? "as" : "ao"));
            for (auto& item : session_objects)
            {
                try {
                    // We check if the caller is allowed to access this
                    // session object.  If not, an exception is thrown
                    // and we will just ignore that exception and continue
                    item.second->CheckACL(sender);
                    if (ret_iface)
                    {
                        g_variant_builder_add(bld, "s", item.second->GetDeviceName().c_str());
                    }
                    else
                    {
                        g_variant_builder_add(bld, "o", item.first.c_str());
                    }
                }
                catch (DBusCredentialsException& excp)
                {
                    // Ignore credentials exceptions.  It means the
                    // caller does not have access this session object
                }
            }

            // Wrap up the result into a tuple, which GDBus expects and
            // put it into the invocation response
            GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(ret, g_variant_builder_end(bld));
            g_dbus_method_invocation_return_value(invoc,
                                                  g_variant_builder_end(ret));

            // Clean-up
            g_variant_builder_unref(bld);
            g_variant_builder_unref(ret);
        }
        else if ("LookupConfigName" == method_name)
        {
            gchar *cfgname_c = nullptr;
            g_variant_get(params, "(s)", &cfgname_c);

            if (nullptr == cfgname_c || strlen(cfgname_c) < 1)
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.name",
                                                              "Invalid configuration name");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            std::string cfgname(cfgname_c);
            g_free(cfgname_c);

            // Build up an array of object paths to sessions with a matching
            // configuration profile name
            GVariantBuilder *found_paths = g_variant_builder_new(G_VARIANT_TYPE("ao"));
            for (const auto& item : session_objects)
            {
                if (item.second->GetConfigName() == cfgname)
                {
                    try
                    {
                        // We check if the caller is allowed to access this
                        // configuration object.  If not, an exception is thrown
                        // and we will just ignore that exception and continue
                        item.second->CheckACL(sender);
                        g_variant_builder_add(found_paths,
                                              "o", item.first.c_str());
                    }
                    catch (DBusCredentialsException& excp)
                    {
                        // Ignore credentials exceptions.  It means the
                        // caller does not have access this configuration object
                    }
                }
            }
            g_dbus_method_invocation_return_value(invoc, GLibUtils::wrapInTuple(found_paths));
            return;
        }
        else if ("LookupInterface" == method_name)
        {
            gchar *iface_c = nullptr;
            g_variant_get(params, "(s)", &iface_c);

            if (nullptr == iface_c || strlen(iface_c) < 1)
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.iface",
                                                              "Invalid interface");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            std::string iface(iface_c);
            g_free(iface_c);

            GVariant *ret = nullptr;
            for (const auto& item : session_objects)
            {
                if (item.second->GetDeviceName() == iface)
                {
                    ret = g_variant_new("(o)", item.first.c_str());
                    break;
                }
            }

            if (nullptr == ret)
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.iface",
                                                              "Interface not found");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            g_dbus_method_invocation_return_value(invoc, ret);
            return;
        }
        else if ("TransferOwnership" == method_name)
        {
            // This feature is quite powerful and is restricted to the
            // root account only.  This is typically used by openvpn3-autoload
            // when run during boot where the auto-load configuration starts
            // a new session automatically wants the owner to be someone else
            // than root.
            if (0 != creds.GetUID(sender))
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.acl.denied",
                                                              "Access Denied");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            gchar *sesspath = nullptr;
            uid_t new_uid = 0;
            g_variant_get(params, "(ou)", &sesspath, &new_uid);

            for (const auto& si : session_objects)
            {
                if (si.first == sesspath)
                {
                    uid_t cur_owner = si.second->GetOwnerUID();
                    si.second->TransferOwnership(new_uid);
                    g_dbus_method_invocation_return_value(invoc, NULL);

                    std::stringstream msg;
                    msg << "Transfered ownership from " << cur_owner
                        << " to " << new_uid
                        << " on session " << sesspath;
                    LogInfo(msg.str());
                    return;
                }
            }
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.path",
                                                          "Invalid session path");
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
            return;
        }
    };


    /**
     *  Callback which is used each time a SessionManagerObject D-Bus
     *  property is being read.
     *
     *  For the SessionManagerObject, this method will just return NULL
     *  with an error set in the GError return pointer.  The
     *  SessionManagerObject does not use properties at all.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns always NULL, as there are no properties in the
     *          SessionManagerObject.
     */
    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        IdleCheck_UpdateTimestamp();
        GVariant *ret = nullptr;

        if ("version" == property_name)
        {
            ret = g_variant_new_string(package_version());
        }
        else
        {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "Unknown property");
        }
        return ret;
    };

    /**
     *  Callback method which is used each time a SessionManagerObject
     *  property is being modified over the D-Bus.
     *
     *  This will always fail with an exception, as there exists no properties
     *  which can be modified in a SessionManagerObject.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Will always throw an execption as there are no properties to
     *         modify.
     */
    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("SessionManagerObject", "set property not implemented");
    }


private:
    GDBusConnection *dbuscon;
    DBusConnectionCreds creds;
    std::map<std::string, SessionObject *> session_objects;

    void remove_session_object(const std::string sesspath)
    {
        uid_t owner = session_objects[sesspath]->GetOwnerUID();
        session_objects.erase(sesspath);

        SessionManager::Event ev{sesspath,
                                 SessionManager::EventType::SESS_DESTROYED,
                                 owner};
        Broadcast(OpenVPN3DBus_interf_sessions,
                  OpenVPN3DBus_rootp_sessions,
                  "SessionManagerEvent",
                  ev.GetGVariant());
    }
};


/**
 * Main D-Bus service implementation of the Session Manager
 */
class SessionManagerDBus : public DBus
{
public:
    /**
     * Constructor creating a D-Bus service for the Session Manager.
     *
     * @param bus_type  GBusType, which defines if this service should be
     *                  registered on the system or session bus.
     * @param logwr     Pointer to LogWriter object; can be nullptr to
     *                  disablefile log.
     *
     */
    SessionManagerDBus(GDBusConnection *conn, LogWriter *logwr,
                       bool signal_broadcast)
        : DBus(conn,
               OpenVPN3DBus_name_sessions,
               OpenVPN3DBus_rootp_sessions,
               OpenVPN3DBus_interf_sessions),
          logwr(logwr),
          signal_broadcast(signal_broadcast),
          managobj(nullptr),
          procsig(nullptr)
    {
        procsig.reset(new ProcessSignalProducer(GetConnection(),
                                                OpenVPN3DBus_interf_sessions,
                                                "SessionManager"));
    };

    ~SessionManagerDBus()
    {
        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
    }


    /**
     *  Sets the log level to use for the session manager main object
     *  and individual session objects.  This is essentially just an
     *  inherited value from the main program.  The SessionManagerObject
     *  should not adjust this for itself.
     *
     *  This does not change the default log level for session objects
     *  themselves, they have a fixed default log level which can be
     *  changed on a per-object basis via a log level property in the object.
     *
     * @param loglvl  Log level to use
     */
    void SetManagerLogLevel(unsigned int loglvl)
    {
        manager_log_level = loglvl;
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        // Create a SessionManagerObject which will be the main entrance
        // point to this service
        managobj.reset(new SessionManagerObject(GetConnection(), GetRootPath(),
                                                manager_log_level, logwr,
                                                signal_broadcast));

        // Register this object to on the D-Bus
        managobj->RegisterObject(GetConnection());

        procsig->ProcessChange(StatusMinor::PROC_STARTED);

        if (nullptr != idle_checker)
        {
            managobj->IdleCheck_Register(idle_checker);
        }
    };


    /**
     *  This is called each time the well-known bus name is successfully
     *  acquired on the D-Bus.
     *
     *  This is not used, as the preparations already happens in
     *  callback_bus_acquired()
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the acquired bus name
     */
    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    };


    /**
     *  This is called each time the well-known bus name is removed from the
     *  D-Bus.  In our case, we just throw an exception and starts shutting
     *  down.
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the lost bus name
     */
    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        THROW_DBUSEXCEPTION("SessionManagerDBus",
                            "openvpn3-service-sessionmgr could not register '"
                            + busname + "' on the D-Bus");
    };

private:
    unsigned int manager_log_level = 6; // LogCategory::DEBUG
    LogWriter *logwr = nullptr;
    bool signal_broadcast = true;
    SessionManagerObject::Ptr managobj;
    ProcessSignalProducer::Ptr procsig;
};

#endif // OPENVPN3_DBUS_SESSIONMGR_HPP
