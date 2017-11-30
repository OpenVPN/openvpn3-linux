//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_DBUS_SESSIONMGR_HPP
#define OPENVPN3_DBUS_SESSIONMGR_HPP

#include <cstring>

#include "openvpn/common/likely.hpp"

#include "common/core-extensions.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/path.hpp"
#include "log/dbus-log.hpp"

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
    SessionManagerSignals(GDBusConnection *conn, std::string object_path)
            : LogSender(conn, LogGroup::SESSIONMGR, OpenVPN3DBus_interf_sessions, object_path)
    {
    }

    virtual ~SessionManagerSignals()
    {
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
        Log(log_group, LogCategory::FATAL, msg);
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
};


/**
 *  Handler for session log events.  This will be enabled when a session
 *  is configured to proxy log messages from the VPN client backend to a
 *  front-end.
 */
class SessionLogEvent : public LogConsumerProxy
{
public:
    /**
     *   Constructor for setting up proxying of log events from a backend to
     *   a front-end.  The interface name of proxied log entries will be
     *   the session managers interface.
     *
     * @param conn               D-Bus connection to use
     * @param bus_name           Backend D-Bus name which will this object
     *                           will subscribe to
     * @param interface          Backend D-Bus interface needed for the
     *                           subscription
     * @param be_obj_path        Backend D-Bus object path where the log
     *                           event signals are sent
     * @param sigproxy_obj_path  Destinaion D-Bus path for the signal
     */
    SessionLogEvent(GDBusConnection *conn,
                    std::string bus_name,
                    std::string interface,
                    std::string be_obj_path,
                    std::string sigproxy_obj_path)
        : LogConsumerProxy(conn, interface, be_obj_path,
                           OpenVPN3DBus_interf_sessions, sigproxy_obj_path)
    {
    }


    /**
     *  A callback method used by LogConsumerProxy(), where we can
     *  intercept log events as they occur.  We use this only to capture
     *  the log event and save a copy of it.
     *
     * @param sender       D-Bus bus name of the sender of the log event
     * @param interface    D-Bus interface of the sender of the log event
     * @param object_path  D-Bus object path of the sender of the log event
     * @param group        LogGroup reference of the log event
     * @param catg         LogCategory reference of the log event
     * @param msg          The log message itself
     */
    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogGroup group, const LogCategory catg,
                         const std::string msg)
    {
        last_group = group;
        last_logcateg = catg;
        last_msg = msg;
    }


    /**
     *  Returns a D-Bus key/value dictionary of the last log message processed
     *
     * @return  Returns a new GVariant Glib2 object containing the log event.
     *          This object needs to be freed when no longer needed.
     */
    GVariant * GetLastLogEntry()
    {
        if( last_msg.empty() && LogGroup::UNDEFINED == last_group)
        {
            return NULL;  // Nothing have been logged, nothing to report
        }
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (b, "{sv}", "log_group", g_variant_new_uint32((guint32) last_group));
        g_variant_builder_add (b, "{sv}", "log_category", g_variant_new_uint32((guint32) last_logcateg));
        g_variant_builder_add (b, "{sv}", "log_message", g_variant_new_string(last_msg.c_str()));
        return g_variant_builder_end(b);
    }


private:
    LogGroup last_group;
    LogCategory last_logcateg;
    std::string last_msg;
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
     *                           subscription
     * @param interface          D-Bus interface to use for the signal
     *                           subscription
     * @param be_obj_path        D-Bus object path of the backend process
     *                           which sends the signals we want to subscribe
     *                           to
     * @param sigproxy_obj_path  D-Bus object path which will be used when
     *                           the session manager sends the proxied
     *                           StatusChange signal
     */
    SessionStatusChange(GDBusConnection *conn,
                        std::string bus_name,
                        std::string interface,
                        std::string be_obj_path,
                        std::string sigproxy_obj_path)
        : DBusSignalSubscription(conn, bus_name, interface, be_obj_path, "StatusChange"),
          DBusSignalProducer(conn, "", OpenVPN3DBus_interf_sessions, sigproxy_obj_path),
          last_major(0),
          last_minor(0),
          last_msg("")
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
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters)
    {
        if (signal_name == "StatusChange")
        {
            gchar *msg;
            g_variant_get (parameters, "(uus)", &last_major, &last_minor, &msg);
            last_msg = std::string(msg);

            /*
              std::cout << "** SessionStatusChange:"
                      << "  sender=" << sender_name
                      << ", path=" << object_path
                      << ", interface=" << interface_name
                      << ", signal=" << signal_name
                      << std::endl
                      << "                      : "
                      << "[" << major << ", " << minor << "] "
                      << msg << std::endl;
            */
            // Proxy this mesage via DBusSignalProducer
            Send("StatusChange", parameters);
        }
    }


    /**
     *  Retrieve the last status message processed
     *
     * @return  Returns a GVariant Glib2 object containing a key/value
     *          dictionary of the last signal sent
     */
    GVariant * GetLastStatusChange()
    {
        if( last_msg.empty() && 0 == last_major && 0 == last_minor)
        {
            return NULL;  // Nothing have been logged, nothing to report
        }
        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (b, "{sv}", "major", g_variant_new_uint32(last_major));
        g_variant_builder_add (b, "{sv}", "minor", g_variant_new_uint32(last_minor));
        g_variant_builder_add (b, "{sv}", "status_message", g_variant_new_string(last_msg.c_str()));
        return g_variant_builder_end(b);
    }


private:
    guint32 last_major;
    guint32 last_minor;
    std::string last_msg;
};


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
     */
    SessionObject(GDBusConnection *dbuscon, uid_t owner, std::string objpath, std::string cfg_path)
        : DBusObject(objpath),
          DBusSignalSubscription(dbuscon, "", OpenVPN3DBus_interf_backends, ""),
          DBusCredentials(dbuscon, owner),
          SessionManagerSignals(dbuscon, objpath),
          be_proxy(nullptr),
          recv_log_events(false),
          config_path(cfg_path),
          sig_statuschg(nullptr),
          sig_logevent(nullptr),
          backend_token(""),
          backend_pid(0),
          be_conn(nullptr),
          log_verb(LogCategory::INFO),
          registered(false),
          selfdestruct_complete(false)
    {
        Subscribe("RegistrationRequest");
        RequiresQueue dummyqueue;  // Only used to get introspection data

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
                          << dummyqueue.IntrospectionMethods("UserInputQueueGetTypeGroup",
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
                          << "        <property type='au' name='acl' access='read'/>"
                          << "        <property type='b' name='public_access' access='readwrite'/>"
                          << "        <property type='s' name='status' access='read'/>"
                          << "        <property type='a{sv}' name='last_log' access='read'/>"
                          << "        <property type='o' name='config_path' access='read'/>"
                          << "        <property type='u' name='backend_pid' access='read'/>"
                          << "        <property type='b' name='receive_log_events' access='readwrite'/>"
                          << "        <property type='u' name='log_verbosity' access='readwrite'/>"
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        // Start a new backend process via the openvpn3-service-backendstart
        // (net.openvpn.v3.backends) service.  A random backend token is
        // created and sent to the backend process.  When the backend process
        // have initialized, it reports back to the session manager using
        // this token as a reference.  This is used to tie the backend process
        // to this specific SessionObject.
        backend_token = generate_path_uuid("", 't');

        auto backend_start = new DBusProxy(G_BUS_TYPE_SYSTEM,
                                           OpenVPN3DBus_name_backends,
                                           OpenVPN3DBus_interf_backends,
                                           OpenVPN3DBus_rootp_backends);
        GVariant *res_g = backend_start->Call("StartClient",
                                              g_variant_new("(s)", backend_token.c_str()));
        if (NULL == res_g) {
                THROW_DBUSEXCEPTION("SessionObject",
                                    "Failed to extract the result of the "
                                    "StartClient request");
        }
        g_variant_get(res_g, "(u)", &backend_pid);

        // The PID value we get here is just a temporary.  This is the
        // PID returned by openvpn3-service-backendstart.  This will again
        // start the openvpn3-service-client process, which will fork() once
        // to be completely independent.  When this last fork() happens,
        // the backend will report back its final PID.
        StatusChange(StatusMajor::SESSION, StatusMinor::PROC_STARTED,
                             "session_path=" + GetObjectPath()
                             + ", backend_pid=" + std::to_string(backend_pid));
        Debug("SessionObject registered on '" + OpenVPN3DBus_interf_sessions + "': "
              + objpath);
    }

    ~SessionObject()
    {
        if (sig_statuschg)
        {
            delete sig_statuschg;
        }

        if (sig_logevent)
        {
            delete sig_logevent;
        }

        if (be_proxy)
        {
            delete be_proxy;
        }
        LogVerb2("Session is closing");
        StatusChange(StatusMajor::SESSION, StatusMinor::SESS_REMOVED);

        IdleCheck_RefDec();
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
            gchar *busn;
            gchar *sesstoken;
            g_variant_get (params, "(ss)", &busn, &sesstoken);

            be_conn = conn;
            be_busname = std::string(busn);
            be_path = std::string(object_path);
            g_free(busn);

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
                Unsubscribe("RegistrationRequest");
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
            StatusMajor major;
            StatusMinor minor;
            gchar *msg;
            g_variant_get (params, "(uus)", &major, &minor, &msg);

            if (StatusMajor::CONNECTION == major
                && StatusMinor::CONN_FAILED == minor)
            {
                // When the backend client signals connection failure
                // force it to shutdown and close this session object
                //
                // FIXME: Consider if we need to split CONN_FAILED into
                //        a fatal error (as now) and "Connection failed, but session may resume"
                //        This link is between here and openvpn-core-client.hpp:78
                //
                shutdown(true);
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
        // std::cout << "SessionObject::callback_method_call: " << method_name << std::endl;
        bool ping = false;
        try {
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
                                    "Backend did not respond to Ping: "
                                    + std::string(dbserr.getRawError()));
            }

            if ("Connect" == method_name)
            {
                CheckACL(sender);
                be_proxy->Call("Connect");
            }
            else if ("Restart" == method_name)
            {
                CheckACL(sender, true);
                be_proxy->Call("Restart");
            }
            else if ("Pause" == method_name)
            {
                CheckACL(sender, true);
                // FIXME: Should check that params contains only the expected formatting
                be_proxy->Call("Pause", params);
            }
            else if ("Resume"  == method_name)
            {
                CheckACL(sender, true);
                be_proxy->Call("Resume");
            }
            else if ("Disconnect" == method_name)
            {
                CheckACL(sender, true);
                shutdown(false);
            }
            else if ("Ready" == method_name)
            {
                CheckACL(sender);
                be_proxy->Call("Ready");
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

                LogVerb1("Access granted to UID " + std::to_string(uid));
                return;
            }
            else if ("AccessRevoke" == method_name)
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                RevokeAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogVerb1("Access revoked for UID " + std::to_string(uid));
                return;
            }
            else
            {
                std::string errmsg = "No method named" + method_name + " is available";
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.sessions.error",
                                                              errmsg.c_str());
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            g_dbus_method_invocation_return_value(invoc, NULL);
        }
        catch (DBusException& dberr)
        {
            bool do_selfdestruct = false;
            std::string errmsg;

            if (!ping)
            {
                try
                {
                    shutdown(true); // Ensure the backend client process have stopped
                }
                catch (DBusException& dberr)
                {
                    // Ignore any errors for now.
                }
                errmsg = "Backend VPN process have died.  Session is no longer valid.";
                if (!selfdestruct_complete)
                {
                    StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, "Backend process died");
                    do_selfdestruct = true;
                }
            }
            else
            {
                errmsg = "Failed communicating with VPN backend: " + dberr.getRawError();
            }

            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.sessions.error",
                                                          errmsg.c_str());
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);

            if (do_selfdestruct)
            {
                selfdestruct(conn);
            }
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
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
        if ("owner" == property_name)
        {
            return GetOwner();
        }

        try
        {
            CheckACL(sender);
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_FAILED);
            return NULL;
        }


        /*
          std::cout << "[SessionObject] get_property(): "
                  << "sender=" << sender
                  << ", object_path=" << obj_path
                  << ", interface=" << intf_name
                  << ", property=" << property_name
                  << std::endl;
        */
        GVariant *ret = NULL;
        if ("receive_log_events" == property_name)
        {
            ret = g_variant_new_boolean (recv_log_events);
        }
        else if ("last_log" == property_name)
        {
            if (nullptr != sig_logevent) {
                ret = sig_logevent->GetLastLogEntry();
                if (NULL == ret)
                {
                    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY,
                                "No data have been logged yet");
                }
            } else {
                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY,
                            "Logging not enabled");
            }
        }
        else if ("status" == property_name)
        {
            ret = NULL;
            if (nullptr != sig_statuschg)
            {
                ret = sig_statuschg->GetLastStatusChange();
            }
            if (NULL == ret)
            {
                g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY,
                            "No status changes have been logged yet");
            }
        }
        else if ("config_path" == property_name)
        {
            ret = g_variant_new_string (config_path.c_str());
        }
        else if ("backend_pid" == property_name)
        {
            ret = g_variant_new_uint32 (backend_pid);
        }
        else if ("log_verbosity" == property_name)
        {
            ret = g_variant_new_uint32 ((guint32) log_verb);
        }
        else if ("public_access" == property_name)
        {
            ret = GetPublicAccess();
        }
        else if ("acl" == property_name)
        {
            ret = GetAccessList();
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
        try
        {
            CheckOwnerAccess(sender);
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.getUserError());
        }

        if (("receive_log_events" == property_name) && be_conn)
        {
            recv_log_events = g_variant_get_boolean(value);
            if (recv_log_events && nullptr == sig_logevent)
            {
                // Subscribe to log signals
                sig_logevent = new SessionLogEvent(
                                be_conn,
                                be_busname,
                                OpenVPN3DBus_interf_backends,
                                be_path,
                                GetObjectPath());
            }
            else if (!recv_log_events && nullptr != sig_logevent)
            {
                delete sig_logevent;
                sig_logevent = nullptr;
            }
            return build_set_property_response(property_name, recv_log_events);
        }
        else if (("log_verbosity" == property_name) && be_conn)
        {
            log_verb = (LogCategory) g_variant_get_uint32(value);

            // FIXME: Proxy log level to the OpenVPN3 Core client
            return build_set_property_response(property_name,
                                               (guint32) log_verb);
        }
        else if (("public_access" == property_name) && conn)
        {
            bool acl_public = g_variant_get_boolean(value);
            SetPublicAccess(acl_public);
            LogVerb1("Public access set to "
                     + (acl_public ? std::string("true") :
                                     std::string("false"))
                     + " by uid " + std::to_string(GetUID(sender)));
            return build_set_property_response(property_name, acl_public);
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

        if (nullptr != sig_logevent)
        {
            delete sig_logevent;
            sig_logevent = nullptr;
        }
    };


private:
    DBusProxy *be_proxy;
    bool recv_log_events;
    std::string config_path;
    SessionStatusChange *sig_statuschg;
    SessionLogEvent *sig_logevent;
    std::string backend_token;
    pid_t backend_pid;
    GDBusConnection *be_conn;
    std::string be_busname;
    std::string be_path;
    LogCategory log_verb;
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
            ping_backend();

            // Setup signal listeneres from the backend process
            // FIXME: Verify how this is related to the subscrition in the caller function
            sig_statuschg = new SessionStatusChange(be_conn,
                                                    be_busname,
                                                    OpenVPN3DBus_interf_backends,
                                                    be_path,
                                                    GetObjectPath());

            GVariant *res_g = be_proxy->Call("RegistrationConfirmation",
                                             g_variant_new("(so)",
                                                           backend_token.c_str(),
                                                           config_path.c_str()));
            if (NULL == res_g) {
                THROW_DBUSEXCEPTION("SessionObject",
                                    "Failed to extract the result of the "
                                    "RegistrationConfirmation response");
            }
            g_variant_get(res_g, "(b)", &registered);
            if (!registered)
            {
                // FIXME: Find a way to gracefully handle failed registration
                return;
            }
            LogVerb1("New session registered: " + GetObjectPath());
            StatusChange(StatusMajor::SESSION, StatusMinor::SESS_NEW,
                         "session_path=" + GetObjectPath()
                         + " backend_busname=" + be_busname
                         + " backend_path=" + be_path);
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
                                "cannot register session");
        }

        bool ret = false;
        g_variant_get(res_g, "(b)", &ret);
        g_variant_unref(res_g);
        return ret;
    }

    /**
     *  Initiate a shutdown of the VPN client backend process.
     *
     * @param forced  If set to True, it will not do a normal disconnect but
     *                tell the backend process to stop more abrubtly.
     */
    void shutdown(bool forced)
    {
        be_proxy->Call( (!forced ? "Disconnect" : "ForceShutdown"), true );
        // Wait for child to exit
        sleep(2); // FIXME: Catch the ProcessChange StatusMinor::PROC_STOPPED signal from backend

        // Remove this session object
        if (!forced)
        {
            StatusChange(StatusMajor::SESSION, StatusMinor::PROC_STOPPED, "Session closed");
        }
        else
        {
            StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, "Session closed, killed backend client");
        }

        selfdestruct(DBusSignalSubscription::GetConnection());
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
                             public SessionManagerSignals
{
public:
    /**
     *  Constructor initializing the SessionManagerObject to be registered on
     *  the D-Bus.
     *
     * @param dbuscon  D-Bus this object is tied to
     * @param objpath  D-Bus object path to this object
     */
    SessionManagerObject(GDBusConnection *dbuscon, const std::string objpath)
        : DBusObject(objpath),
          SessionManagerSignals(dbuscon, objpath),
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
                          << GetLogIntrospection()
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
     * Enables logging to file in addition to the D-Bus Log signal events
     *
     * @param filename  String containing the name of the log file
     */
    void OpenLogFile(std::string filename)
    {
        SessionManagerSignals::OpenLogFile(filename);
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
            gchar *config_path_s;
            g_variant_get (params, "(o)", &config_path_s);
            auto config_path = std::string(config_path_s);
            g_free(config_path_s);

            // Create session object, which will proxy calls
            // from the front-end to the backend
            std::string sesspath = generate_path_uuid(OpenVPN3DBus_rootp_sessions, 's');

            // Create the new object and register it in D-Bus
            SessionObject *session = new SessionObject(conn, creds.GetUID(sender), sesspath, config_path);
            IdleCheck_RefInc();
            session->IdleCheck_Register(IdleCheck_Get());
            session->RegisterObject(conn);

            // Return the path to the new session object object to the caller
            // The backend object will remind "hidden" for the end-user
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(o)", sesspath.c_str()));
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
        /*
        std::cout << "[SessionManagerObject] get_property(): "
                  << "sender=" << sender
                  << ", object_path=" << obj_path
                  << ", interface=" << intf_name
                  << ", property=" << property_name
                  << std::endl;
        */
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Unknown property");
        return NULL;
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
     */
    SessionManagerDBus(GBusType bus_type)
        : DBus(bus_type,
               OpenVPN3DBus_name_sessions,
               OpenVPN3DBus_rootp_sessions,
               OpenVPN3DBus_interf_sessions),
          managobj(nullptr),
          procsig(nullptr),
          logfile("")
    {
    };

    ~SessionManagerDBus()
    {
        delete managobj;

        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
        delete procsig;
    }


    /**
     *  Prepares logging to file.  This happens in parallel with the
     *  D-Bus Log events which will be sent with Log events.
     *
     * @param filename  Filename of the log file to save the log events.
     */
    void SetLogFile(std::string filename)
    {
        logfile = filename;
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        // Create a SessionManagerObject which will be the main entrance
        // point to this service
        managobj = new SessionManagerObject(GetConnection(), GetRootPath());
        if (!logfile.empty())
        {
            managobj->OpenLogFile(logfile);
        }

        // Register this object to on the D-Bus
        managobj->RegisterObject(GetConnection());

        procsig = new ProcessSignalProducer(GetConnection(),
                                            OpenVPN3DBus_interf_sessions,
                                            "SessionManager");
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
        THROW_DBUSEXCEPTION("SessionManagerDBus", "Session Manager's D-Bus name not registered: '" + busname + "'");
    };

private:
    SessionManagerObject * managobj;
    ProcessSignalProducer * procsig;
    std::string logfile;
};

#endif // OPENVPN3_DBUS_SESSIONMGR_HPP
