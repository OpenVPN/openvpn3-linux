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

    void LogFATAL(std::string msg)
    {
        Log(log_group, LogCategory::FATAL, msg);
        StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, msg);
        abort();
    }


    void Debug(std::string msg)
    {
            LogSender::Debug(msg);
    }

    void Debug(std::string busname, std::string path, pid_t pid, std::string msg)
    {
            std::stringstream debug;
            debug << "pid=" << std::to_string(pid)
                  << ", busname=" << busname
                  << ", path=" << path
                  << ", message=" << msg;
            LogSender::Debug(debug.str());
    }

    void StatusChange(const StatusMajor major, const StatusMinor minor, std::string msg)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, msg.c_str());
        Send("StatusChange", params);
    }

    void StatusChange(const StatusMajor major, const StatusMinor minor)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, "");
        Send("StatusChange", params);
    }
};


class SessionLogEvent : public LogConsumerProxy
{
public:
    SessionLogEvent(GDBusConnection *conn,
                    std::string bus_name,
                    std::string interface,
                    std::string be_obj_path,
                    std::string sigproxy_obj_path)
        : LogConsumerProxy(conn, interface, be_obj_path,
                           OpenVPN3DBus_interf_sessions, sigproxy_obj_path)
    {
    }


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


class SessionStatusChange : public DBusSignalSubscription,
                            public DBusSignalProducer
{
public:
    SessionStatusChange(GDBusConnection *conn,
                        std::string bus_name,
                        std::string interface,
                        std::string be_obj_path,
                        std::string sigproxy_obj_path,
                        std::string signal_name)
        : DBusSignalSubscription(conn, bus_name, interface, be_obj_path, signal_name),
          DBusSignalProducer(conn, "", OpenVPN3DBus_interf_sessions, sigproxy_obj_path),
          last_major(0),
          last_minor(0),
          last_msg("")
    {
    }

    void callback_signal_handler(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters)
    {
        if( 0 == g_strcmp0(signal_name, "StatusChange"))
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


class SessionObject : public DBusObject,
                      public DBusSignalSubscription,
                      public DBusCredentials,
                      public SessionManagerSignals
{
public:
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

        // Create the backend session object, which the management process uses to
        // control the client session process which will be forked out from here
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


    void callback_signal_handler(GDBusConnection *conn,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *params)
    {
        if (0 == g_strcmp0(signal_name, "RegistrationRequest")
            && (0 == g_strcmp0(interface_name, OpenVPN3DBus_interf_backends.c_str())))
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
        else if ((0 ==  g_strcmp0(signal_name, "StatusChange"))
                 && (0 == g_strcmp0(interface_name, OpenVPN3DBus_interf_backends.c_str())))
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
        else if ((0 ==  g_strcmp0(signal_name, "AttentionRequired"))
                 && (0 == g_strcmp0(interface_name, OpenVPN3DBus_interf_backends.c_str())))
        {
                Send("AttentionRequired", params);
        }
    }

    void callback_method_call(GDBusConnection *conn,
                                          const gchar *sender,
                                          const gchar *obj_path,
                                          const gchar *intf_name,
                                          const gchar *meth_name,
                                          GVariant *params,
                                          GDBusMethodInvocation *invoc)
    {
        // std::cout << "SessionObject::callback_method_call: " << meth_name << std::endl;
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

            if (0 == g_strcmp0(meth_name, "Connect"))
            {
                CheckACL(sender);
                be_proxy->Call("Connect");
            }
            else if (0 == g_strcmp0(meth_name, "Restart"))
            {
                CheckACL(sender, true);
                be_proxy->Call("Restart");
            }
            else if (0 == g_strcmp0(meth_name, "Pause"))
            {
                CheckACL(sender, true);
                // FIXME: Should check that params contains only the expected formatting
                be_proxy->Call("Pause", params);
            }
            else if (0 == g_strcmp0(meth_name, "Resume"))
            {
                CheckACL(sender, true);
                be_proxy->Call("Resume");
            }
            else if (0 == g_strcmp0(meth_name, "Disconnect"))
            {
                CheckACL(sender, true);
                shutdown(false);
            }
            else if (0 == g_strcmp0(meth_name ,"Ready"))
            {
                CheckACL(sender);
                be_proxy->Call("Ready");
            }
            else if (0 == g_strcmp0(meth_name ,"UserInputQueueGetTypeGroup"))
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
            else if (0 == g_strcmp0(meth_name ,"UserInputQueueFetch"))
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
            else if (0 == g_strcmp0(meth_name ,"UserInputQueueCheck"))
            {
                CheckACL(sender);
                GVariant *res = be_proxy->Call("UserInputQueueCheck", params);
                g_dbus_method_invocation_return_value(invoc, res);
                g_variant_unref(res);
                return;
            }
            else if (0 == g_strcmp0(meth_name ,"UserInputProvide"))
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
            else if (0 == g_strcmp0(meth_name, "AccessGrant"))
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                GrantAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogVerb1("Access granted to UID " + std::to_string(uid));
                return;
            }
            else if (0 == g_strcmp0(meth_name, "AccessRevoke"))
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
                std::string errmsg = "No method named" + std::string(meth_name) + " is available";
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


    GVariant * callback_get_property (GDBusConnection *conn,
                                      const gchar *sender,
                                      const gchar *obj_path,
                                      const gchar *intf_name,
                                      const gchar *property_name,
                                      GError **error)
    {
        if( 0 == g_strcmp0(property_name, "owner") )
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
        if (0 == g_strcmp0(property_name, "receive_log_events") )
        {
            ret = g_variant_new_boolean (recv_log_events);
        }
        else if (0 == g_strcmp0(property_name, "last_log") )
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
        else if (0 == g_strcmp0(property_name, "status") )
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
        else if (0 == g_strcmp0(property_name, "config_path") )
        {
            ret = g_variant_new_string (config_path.c_str());
        }
        else if (0 == g_strcmp0(property_name, "backend_pid") )
        {
            ret = g_variant_new_uint32 (backend_pid);
        }
        else if (0 == g_strcmp0(property_name, "log_verbosity") )
        {
            ret = g_variant_new_uint32 ((guint32) log_verb);
        }
        else if( 0 == g_strcmp0(property_name, "public_access") )
        {
            ret = GetPublicAccess();
        }
        else if( 0 == g_strcmp0(property_name, "acl"))
        {
                ret = GetAccessList();
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

    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const gchar *sender,
                                            const gchar *obj_path,
                                            const gchar *intf_name,
                                            const gchar *property_name,
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

        if (0 == g_strcmp0(property_name, "receive_log_events") && be_conn)
        {
            recv_log_events =  g_variant_get_boolean(value);
            if (recv_log_events && nullptr == sig_logevent)
            {
                // Subscribe to log signals
                sig_logevent = new SessionLogEvent(be_conn,
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
        else if (0 == g_strcmp0(property_name, "log_verbosity") && be_conn)
        {
            log_verb =  (LogCategory) g_variant_get_uint32(value);

            // FIXME: Proxy log level to the OpenVPN3 Core client
            return build_set_property_response(property_name, (guint32) log_verb);
        }
        else if (0 == g_strcmp0(property_name, "public_access") && conn)
        {
            bool acl_public = g_variant_get_boolean(value);
            SetPublicAccess(acl_public);
            LogVerb1("Public access set to "
                             + (acl_public ? std::string("true") : std::string("false"))
                             + " by uid " + std::to_string(GetUID(sender)));
            return build_set_property_response(property_name, acl_public);
        }

        throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                    obj_path, intf_name, property_name,
                                    "Invalid property");
    }


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
                                                    GetObjectPath(),
                                                    "StatusChange");

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



class SessionManagerObject : public DBusObject,
                             public SessionManagerSignals
{
public:
    SessionManagerObject(GDBusConnection *dbuscon, const std::string busname, const std::string objpath)
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

    void OpenLogFile(std::string filename)
    {
        SessionManagerSignals::OpenLogFile(filename);
    }

    void callback_method_call(GDBusConnection *conn,
                              const gchar *sender,
                              const gchar *obj_path,
                              const gchar *intf_name,
                              const gchar *meth_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        // std::cout << "SessionManagerObject::callback_method_call: " << meth_name << std::endl;
        if (0 == g_strcmp0(meth_name, "NewTunnel"))
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


    GVariant * callback_get_property (GDBusConnection *conn,
                                      const gchar *sender,
                                      const gchar *obj_path,
                                      const gchar *intf_name,
                                      const gchar *property_name,
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


    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                                   const gchar *sender,
                                                   const gchar *obj_path,
                                                   const gchar *intf_name,
                                                   const gchar *property_name,
                                                   GVariant *value,
                                                   GError **error)
    {
        THROW_DBUSEXCEPTION("SessionManagerObject", "set property not implemented");
    }


private:
    GDBusConnection *dbuscon;
    DBusConnectionCreds creds;
};


class SessionManagerDBus : public DBus
{
public:
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

    void SetLogFile(std::string filename)
    {
        logfile = filename;
    }

    void callback_bus_acquired()
    {
        managobj = new SessionManagerObject(GetConnection(), GetBusName(), GetRootPath());
        if (!logfile.empty())
        {
            managobj->OpenLogFile(logfile);
        }
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

    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    };

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

