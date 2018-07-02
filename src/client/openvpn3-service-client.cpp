//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
 * @file   openvpn3-service-client.cpp
 *
 * @brief  Service side implementation the OpenVPN 3 based VPN client
 *
 *         This service is supposed to be started by the
 *         openvpn3-service-backendstart service.  One client service
 *         represents a single VPN tunnel and is managed only by the session
 *         manager service.  When starting, this service will signal the
 *         session manager about its presence and the session manager will
 *         respond with which configuration profile to use.  Once that is done
 *         the front-end instance (communicating with the session manager)
 *         can continue with providing credentials and start the tunnel
 *         connection.
 */

#include <sstream>

#define SHUTDOWN_NOTIF_PROCESS_NAME "openvpn3-service-client"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "common/cmdargparser.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "dbus/core.hpp"
#include "dbus/path.hpp"
#include "log/dbus-log.hpp"
#include "backend-signals.hpp"
#include "core-client.hpp"

using namespace openvpn;

/**
 *  Class managing a specific VPN client tunnel.  This object has its own
 *  unique D-Bus bus name and object path and is designed to only be
 *  accessible by the user running the openvpn3-service-sessiongmr process.
 *  This session manager is the front-end users access point to this object.
 */
class BackendClientObject : public DBusObject,
                            public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<BackendClientObject> Ptr;

    /**
     *  Initialize the BackendClientObject.  The bus name this object is
     *  tied to is based on the PID value of this client process.
     *
     * @param conn           D-Bus connection this object is tied to
     * @param bus_name       Unique D-Bus bus name
     * @param objpath        D-Bus object path where to reach this instance
     * @param session_token  String based token which is used to register
     *                       itself with the session manager.  This token
     *                       is provided on the command line when starting
     *                       this openvpn3-service-client process.
     */
    BackendClientObject(GDBusConnection *conn, std::string bus_name,
                         std::string objpath, std::string session_token,
                         unsigned int default_log_level)
        : DBusObject(objpath),
          dbusconn(conn),
          mainloop(nullptr),
          signal(conn, LogGroup::CLIENT, objpath),
          session_token(session_token),
          registered(false),
          paused(false),
          vpnclient(nullptr),
          client_thread(nullptr)
    {
        // Initialize the VPN Core
        CoreVPNClient::init_process();

        signal.SetLogLevel(default_log_level);

        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_backends << "'>"
                          << "        <method name='RegistrationConfirmation'>"
                          << "            <arg type='s' name='token' direction='in'/>"
                          << "            <arg type='o' name='config_path' direction='in'/>"
                          << "            <arg type='b' name='response' direction='out'/>"
                          << "        </method>"
                          << "        <method name='Ping'>"
                          << "            <arg type='b' name='alive' direction='out'/>"
                          << "        </method>"
                          << "        <method name='Ready'/>"
                          << "        <method name='Connect'/>"
                          << "        <method name='Pause'>"
                          << "            <arg type='s' name='reason' direction='in'/>"
                          << "        </method>"
                          << "        <method name='Resume'/>"
                          << "        <method name='Restart'/>"
                          << "        <method name='Disconnect'/>"
                          << "        <method name='ForceShutdown'/>"
                          << userinputq.IntrospectionMethods("UserInputQueueGetTypeGroup",
                                                             "UserInputQueueFetch",
                                                             "UserInputQueueCheck",
                                                             "UserInputProvide")
                          << "        <property name='log_level' type='u' access='readwrite'/>"
                          << signal.GetStatusChangeIntrospection()
                          << signal.GetLogIntrospection()
                          << "        <signal name='AttentionRequired'>"
                          << "            <arg type='u' name='type' direction='out'/>"
                          << "            <arg type='u' name='group' direction='out'/>"
                          << "            <arg type='s' name='message' direction='out'/>"
                          << "        </signal>"
                          << "        <signal name='RegistrationRequest'>"
                          << "            <arg type='s' name='busname' direction='out'/>"
                          << "            <arg type='s' name='token' direction='out'/>"
                          << "        </signal>"
                          << "        <property type='a{sx}' name='statistics' access='read'/>"
                          << "        <property type='a{sv}' name='status' access='read'/>"
                          <<  "    </interface>"
                          <<  "</node>";
        ParseIntrospectionXML(introspection_xml);

        // Tell the session manager we are ready.  This
        // request will also carry the correct object path
        // in the response automatically, but the well-known
        // bus name needs to be sent back.
        signal.Debug("Sending RegistrationRequest('" + bus_name + "', '" + session_token + "') signal");
        signal.Send(OpenVPN3DBus_name_sessions,
                    OpenVPN3DBus_interf_backends,
                    "RegistrationRequest",
                    g_variant_new("(ss)", bus_name.c_str(), session_token.c_str()));
    }


    ~BackendClientObject()
    {
        CoreVPNClient::uninit_process();
    }


    /**
     *  Provides a reference to the Glib2 main loop object.  This is used
     *  to cleanly shutdown this process when the session manager wants to
     *  shutdown this process via D-Bus.
     *
     * @param ml   GMainLoop pointer to the current main loop thread
     */
    void SetMainLoop(GMainLoop *ml)
    {
        mainloop = ml;
    }


    /**
     *  Callback method which is called each time a D-Bus method call occurs
     *  on this BackendClientObject.
     *
     * @param conn        D-Bus connection where the method call occurred
     * @param sender      D-Bus bus name of the sender of the method call
     * @param obj_path    D-Bus object path of the target object.
     * @param intf_name   D-Bus interface of the method call
     * @param method_name D-Bus method name to be executed
     * @param params      GVariant Glib2 object containing the arguments for
     *                    the method call
     * @param invoc       GDBusMethodInvocation where the response/result of
     *                    the method call will be returned.
     */
    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        // Ensure D-Bus method calls are serialized
        std::lock_guard<std::mutex> lg(guard);

        try
        {
            // Ensure a vpnclient object is present only when we are
            // expected to be in an active connection.
            if (vpnclient)
            {
                switch(vpnclient->GetRunStatus())
                {
                case StatusMinor::CFG_REQUIRE_USER: // Requires reconnect
                case StatusMinor::CONN_DISCONNECTED:
                case StatusMinor::CONN_FAILED:
                    // When a connection have been torn down,
                    // we need to re-establish the client object
                    vpnclient = nullptr;
                    break;
                default:
                    break;
                }
            }

            if ("RegistrationConfirmation" == method_name)
            {
                // This is called by the session manager only, as an
                // acknowledgement from the session manager that it has
                // linked this client process to a valid session object which
                // will be accessible for front-end users.
                //
                // With this call, we also get the D-Bus object path for the
                // the VPN configuration profile to use.  This is used when
                // retrieve the configuration profile from the configuration
                // manager service through the fetch_configuration() call.
                //
                if (registered)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject",
                                        "Backend service is already registered");
                }

                gchar *token = NULL;
                gchar *cfgpath = NULL;
                g_variant_get (params, "(so)", &token, &cfgpath);

                registered = (session_token == std::string(token));
                configpath = std::string(cfgpath);

                signal.Debug("Registration confirmation: "
                             + std::string(token) + " == "
                             + std::string(session_token) + " => "
                             + (registered ? "true" : "false"));
                if (registered)
                {
                    g_dbus_method_invocation_return_value(invoc,
                                                          g_variant_new("(b)", (bool) registered));

                    // Fetch the configuration from the config-manager.
                    // Since the configuration may be set up for single-use
                    // only, we must keep this config as long as we're running
                    fetch_configuration();

                    // Sets initial state, which also allows us to early
                    // report back back if more data is required to be
                    // sent by the front-end interface.
                    initialize_client();
                }
                else
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.be-registration",
                                                                  "Invalid registration token");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                }
                return;
            }
            else if ("Ping" == method_name)
            {
                // This is a more narrow Ping test than what the D-Bus
                // infrastructure provides.  This is a ping response from this
                // specific object.
                //
                // The Ping caller is expected to just receive true.
                g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", (bool) true));
                return;
            }
            else if ("Ready" == method_name)
            {
                // This method should just exit without any result if everything is okay.
                // If there are issues, return an error message
                if (!userinputq.QueueAllDone())
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.ready",
                                                                  "Missing user credentials");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                }
            }
            else if ("Connect" == method_name)
            {
                // This starts the connection against a VPN server

                if( !registered )
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                // This re-initializes the client object.  If we have already
                // tried to connectbut got an AUTH_FAILED, either due to wrong
                // credentials or a dynamic challenge from the server, we
                // need to re-establish the vpnclient object.
                initialize_client();

                if (!userinputq.QueueAllDone())
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.backend",
                                                                  "Required user input not provided");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }
                signal.LogInfo("Starting connection: " + to_string(obj_path));
                connect();
            }
            else if ("Disconnect" == method_name)
            {
                // Disconnect from the server.  This will also shutdown this
                // process.

                if (!registered || !vpnclient)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                signal.LogInfo("Stopping connection: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_DISCONNECTING);
                vpnclient->stop();
                if (client_thread)
                {
                    client_thread->join();
                }
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_DONE);

                // Shutting down our selves.
                RemoveObject(dbusconn);
                if (mainloop)
                {
                    g_main_loop_quit(mainloop);
                }
                else
                {
                    kill(getpid(), SIGTERM);
                }
            }
            else if ("UserInputQueueGetTypeGroup"  == method_name)
            {
                // Return an array of tuples of ClientAttentionTypes and
                // ClientAttentionGroups which needs to be satisfied before
                // we can attempt another reconnect.  This is all handled
                // by the RequiresQueue.

                try
                {
                    userinputq.QueueCheckTypeGroup(invoc);
                }
                catch (RequiresQueueException& excp)
                {
                    excp.GenerateDBusError(invoc);
                }
                return; // QueueCheckTypeGroup() have fed invoc with a result already
            }
            else if ("UserInputQueueFetch"  == method_name)
            {
                // Retrieves a specific RequiresQueue item which the front-end
                // needs to satisfy.

                try
                {
                    userinputq.QueueFetch(invoc, params);
                }
                catch (RequiresQueueException& excp)
                {
                    excp.GenerateDBusError(invoc);
                }
                return; // QueueFetch() have fed invoc with a result already
            }
            else if ("UserInputQueueCheck" == method_name)
            {
                // Retrieve the RequiresSlot IDs for a specific
                // ClientAttentionType/ClientAttentionGroup which needs to be
                // satisfied by the front-end.

                userinputq.QueueCheck(invoc, params);
                return; // QueueCheck() have fed invoc with a result already
            }
            else if ("UserInputProvide" == method_name)
            {
                // This is called each time a RequiresSlot gets an update
                // with data from the front-end.

                if (!registered)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                if (userinputq.QueueDone(params))
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.backend",
                                                                  "Credentials not needed");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }
                userinputq.UpdateEntry(invoc, params);
            }
            else if ("Pause" == method_name)
            {
                // Pauses and suspends an on-going and connected VPN tunnel.
                // The reason message provided with this call is sent to the
                // log.

                if( !registered || !vpnclient )
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                if (paused)
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.backend",
                                                                  "Connection is already paused");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }

                gchar *reason_str = NULL;
                g_variant_get (params, "(s)", &reason_str);
                std::string reason(reason_str);

                signal.LogInfo("Pausing connection: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_PAUSING,
                                    "Reason: " + reason);
                vpnclient->pause(reason);
                paused = true;
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_PAUSED);
            }
            else if ("Resume" == method_name)
            {
                // Resumes an already paused VPN session

                if( !registered || !vpnclient )
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                if (!paused)
                {
                    GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.backend",
                                                                  "Connection is not paused");
                    g_dbus_method_invocation_return_gerror(invoc, err);
                    g_error_free(err);
                    return;
                }

                signal.LogInfo("Resuming connection: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_RESUMING);
                vpnclient->resume();
                paused = false;
            }
            else if ("Restart" == method_name)
            {
                // Does a complete re-connect for an already running VPN
                // session.  This will reuse all the credentials already
                // gathered.

                if (!registered || !vpnclient)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }
                signal.LogInfo("Restarting connection: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_RECONNECTING);
                vpnclient->reconnect(0);
            }
            else if ("ForceShutdown" == method_name)
            {
                // This is an emergency break for this process.  This
                // kills this process without considering if we are in
                // an already running state.  This is primarily used to
                // clean-up stray session objects which is considered dead
                // by the session manager.

                signal.LogInfo("Forcing shutdown of backend process: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_DONE);

                // Shutting down our selves.
                RemoveObject(dbusconn);
                if (mainloop)
                {
                    g_main_loop_quit(mainloop);
                }
                else
                {
                    kill(getpid(), SIGTERM);
                }
            }
            else
            {
                throw std::invalid_argument("Not implemented method");
            }
            g_dbus_method_invocation_return_value(invoc, NULL);
        }
        catch (const std::exception& excp)
        {
            std::string errmsg = "Failed executing D-Bus call '" + method_name + "': " + excp.what();
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.backend.error.standard",
                                                          errmsg.c_str());
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
        }
        catch (...)
        {
            GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.backend.error.unspecified",
                                                          "Unknown error");
            g_dbus_method_invocation_return_gerror(invoc, err);
            g_error_free(err);
        }
    }

    /**
     *   Callback which is used each time a BackendClientObject D-Bus property
     *   is being read.
     *
     *   Only the session manager instance should be allowed to read
     *   the properties this D-Bus object provides.
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
        // Access to properties are controled by the D-Bus policy.
        // Normally only the session manager should have access to
        // to these properties.
        if ("statistics" == property_name)
        {
            // Returns the current statistics for a running and connected
            // VPN session

            // Returns an array of a string (description) and an int64
            // containing the statistics value.
            GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sx}"));
            for (auto& sd : vpnclient->GetStats())
            {
                g_variant_builder_add (b, "{sx}",
                                       sd.key.c_str(), sd.value);
            }
            GVariant *ret = g_variant_builder_end(b);
            g_variant_builder_unref(b);
            return ret;
        }
        else if ("status" == property_name)
        {
            return signal.GetLastStatusChange();
        }
        else if ("log_level" == property_name)
        {
            return g_variant_new_uint32(signal.GetLogLevel());
        }
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property");
        return NULL;
    }

    /**
     *  Callback method which is used each time a BackendClientObject
     *  property is being modified over the D-Bus.
     *
     *  This will always fail with an exception, as there exists no properties
     *  which can be modified in a BackendClientObject object.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param value          GVariant object containing the value to be stored
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return Will always throw an exception as there are no properties to
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
        try
        {
            // Access to properties are controled by the D-Bus policy.
            // Normally only the session manager should have access to
            // to these properties.
            if ("log_level" == property_name)
            {
                unsigned int log_verb = g_variant_get_uint32(value);
                signal.SetLogLevel(log_verb);
                return build_set_property_response(property_name,
                                                   (guint32) log_verb);
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


private:
    GDBusConnection *dbusconn;
    GMainLoop *mainloop;
    BackendSignals signal;
    std::string session_token;
    bool registered;
    bool paused;
    std::string configpath;
    CoreVPNClient::Ptr vpnclient;
    std::thread * client_thread;
    ClientAPI::Config vpnconfig;
    ClientAPI::EvalConfig cfgeval;
    ClientAPI::ProvideCreds creds;
    RequiresQueue userinputq;
    std::mutex guard;


    /**
     *  This implements the POSIX thread running the CoreVPNClient session
     */
    void run_connection_thread()
    {
        asio::detail::signal_blocker sigblock; // Block signals in client thread

        try
        {
            signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_CONNECTING, "");
            ClientAPI::Status status = vpnclient->connect();
            if (status.error)
            {
                std::stringstream msg;
                msg << status.message;
                if (!status.status.empty())
                {
                    msg << " {" << status.status << "}";
                }
                signal.LogError("Connection failed: " + status.message);
                signal.Debug("Connection failed: " + msg.str());
                signal.StatusChange(StatusMajor::CONNECTION,
                                    StatusMinor::CONN_FAILED,
                                    msg.str());
            }
        }
        catch (openvpn::Exception& excp)
        {
            signal.LogFATAL(excp.what());
        }
   }


    /**
     *  Starts a new POSIX thread which will run the
     *  VPN client (CoreVPNClient)
     */
    void connect()
    {
        try
        {
            if (!registered || !vpnclient)
            {
                THROW_DBUSEXCEPTION("BackendServiceObject",
                                    "Session object not properly registered or not initialized");
            }

            // FIXME: Seems redundant with what is in call_
            if (!userinputq.QueueAllDone())
            {
                THROW_DBUSEXCEPTION("BackendServiceObject",
                                    "Required user input needs to be provided first");
            }

            bool provide_creds = false;
            if (userinputq.QueueCount(ClientAttentionType::CREDENTIALS,
                                      ClientAttentionGroup::USER_PASSWORD) > 0)
            {
                creds.username = userinputq.GetResponse(ClientAttentionType::CREDENTIALS,
                                                        ClientAttentionGroup::USER_PASSWORD,
                                                        "username");
                if (userinputq.QueueCount(ClientAttentionType::CREDENTIALS,
                                          ClientAttentionGroup::CHALLENGE_DYNAMIC) == 0)
                {
                    creds.password = userinputq.GetResponse(ClientAttentionType::CREDENTIALS,
                                                            ClientAttentionGroup::USER_PASSWORD,
                                                            "password");
                    creds.cachePassword = true;
                }
                creds.replacePasswordWithSessionID = true; // If server sends auth-token
                provide_creds = true;
            }

            if (userinputq.QueueCount(ClientAttentionType::CREDENTIALS,
                                      ClientAttentionGroup::CHALLENGE_DYNAMIC) > 0)
            {
                creds.dynamicChallengeCookie = userinputq.GetResponse(ClientAttentionType::CREDENTIALS,
                                                        ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                                        "dynamic_challenge_cookie");
                creds.response = userinputq.GetResponse(ClientAttentionType::CREDENTIALS,
                                                        ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                                        "dynamic_challenge");
                provide_creds = true;
            }

            if (provide_creds)
            {
                ClientAPI::Status cred_res = vpnclient->provide_creds(creds);
                if (cred_res.error)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject",
                                        "Credentials error: " + cred_res.message);
                }

                std::stringstream msg;
                msg << "Username/password provided successfully"
                    << " for '" << creds.username << "'";
                signal.LogVerb1(msg.str());
                if (!creds.response.empty())
                {
                    std::stringstream dmsg;
                    dmsg << "Dynamic challenge provided successfully"
                         << " for '" << creds.username << "'";
                    signal.LogVerb1(dmsg.str());
                }
            }

            // Start client thread
            client_thread = new std::thread([self=Ptr(this)]()
                                                {
                                                    self->run_connection_thread();
                                                }
                                               );
        }
        catch(const DBusException& err)
        {
            signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::PROC_KILLED,
                                err.what());
            signal.LogFATAL("Client thread exception: " + std::string(err.what()));
            THROW_DBUSEXCEPTION("BackendServiceObject", std::string(err.what()));
        }

    }


    /**
     *   Initializes a new CoreVPNClient object
     */
    void initialize_client()
    {
        if (vpnconfig.content.empty())
        {
            THROW_DBUSEXCEPTION("BackendServiceObject",
                                "No configuration profile has been parsed");
        }

        // Create a new VPN client object, which is handling the
        // tunnel itself.
        vpnclient.reset(new CoreVPNClient(&signal, &userinputq));

        // We need to provide a copy of the vpnconfig object, as vpnclient
        // seems to take ownership
        cfgeval = vpnclient->eval_config(ClientAPI::Config(vpnconfig));
        if (cfgeval.error)
        {
            std::stringstream statusmsg;
            statusmsg << "config_path=" << configpath << ", "
                      << "eval_message='" << cfgeval.message << "', "
                      << "error_message='" << cfgeval.error << "'";
            signal.LogError("Failed to parse configuration: " + cfgeval.error);
            signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CFG_ERROR,
                                statusmsg.str());
            signal.Debug(statusmsg.str());
            vpnclient = nullptr;
            THROW_DBUSEXCEPTION("BackendServiceObject",
                                "Configuration parsing failed: " + cfgeval.error);
        }

        // Do we need username/password?  Or does this configuration allow the
        // client to log in automatically?
        if (!cfgeval.autologin
            && userinputq.QueueCount(ClientAttentionType::CREDENTIALS,
                                     ClientAttentionGroup::USER_PASSWORD) == 0)
        {
            // FIXME: Consider to have --auth-nocache approach as well
            userinputq.RequireAdd(ClientAttentionType::CREDENTIALS,
                                  ClientAttentionGroup::USER_PASSWORD,
                                  "username", "Auth User name", false);
            userinputq.RequireAdd(ClientAttentionType::CREDENTIALS,
                                  ClientAttentionGroup::USER_PASSWORD,
                                  "password", "Auth Password", true);
            signal.AttentionReq(ClientAttentionType::CREDENTIALS,
                                ClientAttentionGroup::USER_PASSWORD,
                                "Username/password credentials needed");
        }

        signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CFG_OK,
                            "config_path=" + configpath);
    }


    /**
     *  Retrieves the VPN configuration profile from the configuration
     *  manager.
     *
     *  The configuration is cached in this object as the configuration might
     *  have the 'single_use' attribute set, which means we can only retrieve
     *  it once from the configuration manager.
     */
    void fetch_configuration()
    {
        // Retrieve confniguration
        signal.LogVerb2("Retrieving configuration from " + configpath);

        try
        {
            auto cfg_proxy = OpenVPN3ConfigurationProxy(G_BUS_TYPE_SYSTEM,
                                                        configpath);

            // We need to extract the persist_tun property *before* calling
            // GetConfig().  If the configuration is tagged as a single-shot
            // config, we cannot query it for more details after the first
            // GetConfig() call.
            bool tunPersist = cfg_proxy.GetPersistTun();

            // Parse the configuration
            ProfileMergeFromString pm(cfg_proxy.GetConfig(), "",
                                      ProfileMerge::FOLLOW_NONE,
                                      ProfileParseLimits::MAX_LINE_SIZE,
                                      ProfileParseLimits::MAX_PROFILE_SIZE);
#ifdef CONFIGURE_GIT_REVISION
            vpnconfig.guiVersion = openvpn::platform_string(PACKAGE_NAME, "git:" CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS);
#else
            vpnconfig.guiVersion = openvpn::platform_string(PACKAGE_NAME, PACKAGE_GUIVERSION);
#endif
            vpnconfig.info = true;
            vpnconfig.content = pm.profile_content();
            vpnconfig.tunPersist = tunPersist;
        }
        catch (std::exception& e)
        {
            // This should normally not happen
            signal.LogFATAL("** EXCEPTION ** openvpn3-service-client/fetch_config():"
                            + std::string(e.what()));
        }
    }
};



/**
 *  Main Backend Client D-Bus service.  This registers this client process
 *  as a separate and unique D-Bus service
 */
class BackendClientDBus : public DBus
{
public:
    /**
     *  Initializes the BackendClientDBus object
     *
     * @param start_pid  The PID value we were started with, used for logging
     * @param bus_type   GBusType, which defines if this service should be
     *                   registered on the system or session bus.
     * @param sesstoken  String containing the session token provided via the
     *                   command line.  This is used when signalling back
     *                   to the session manager.
     */
    BackendClientDBus(pid_t start_pid, GBusType bus_type, std::string sesstoken)
        : DBus(bus_type,
               OpenVPN3DBus_name_backends_be + to_string(getpid()),
               OpenVPN3DBus_rootp_sessions,
               OpenVPN3DBus_interf_sessions),
          start_pid(start_pid),
          session_token(sesstoken),
          procsig(nullptr),
          be_obj(nullptr),
          signal(nullptr)
    {
    };

    ~BackendClientDBus()
    {
        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
        delete procsig;
        //delete be_obj;
    }


    /**
     *  Provides a reference to the Glib2 main loop object.  This is used
     *  to cleanly shutdown this process when the session manager wants to
     *  shutdown this process via D-Bus.
     *
     * @param ml   GMainLoop pointer to the current main loop thread
     */
    void SetMainLoop(GMainLoop *ml)
    {
        if (be_obj)
        {
            be_obj->SetMainLoop(ml);
        }
    }

    /**
     *  Sets the default log level when the backend client starts.  This
     *  can later on be adjusted by modifying the log_level D-Bus object
     *  property.  When not being changed, the default log level is 6.
     *
     * @param lvl  Unsigned integer of the default log level.
     */
    void SetDefaultLogLevel(unsigned int lvl)
    {
        default_log_level = lvl;
    }


    /**
     *  Prepares logging to file.  This happens in parallel with the
     *  D-Bus Log events which will be sent with Log events.
     *
     * @param filename  Filename of the log file to save the log events.
     */
    void LogFile(std::string logfile)
    {
        signal->OpenLogFile(logfile);
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        // Create a new OpenVPN3 client session object
        object_path = generate_path_uuid(OpenVPN3DBus_rootp_backends_sessions, 'z');
        be_obj.reset(new BackendClientObject(GetConnection(), GetBusName(),
                                             object_path,
                                             session_token,
                                             default_log_level));
        be_obj->RegisterObject(GetConnection());

        // Setup a signal object of the backend
        signal = new BackendSignals(GetConnection(), LogGroup::BACKENDPROC, object_path);
        signal->SetLogLevel(default_log_level);
        signal->LogVerb2("Backend client process started as pid " + std::to_string(start_pid)
                         + " re-initiated as pid " + std::to_string(getpid()));
        signal->Debug("BackendClientDBus registered on '" + GetBusName()
                       + "': " + object_path);

        procsig = new ProcessSignalProducer(GetConnection(), OpenVPN3DBus_interf_backends,
                                            object_path, "VPN-Client");
        procsig->ProcessChange(StatusMinor::PROC_STARTED);
    }


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
        THROW_DBUSEXCEPTION("BackendClientDBus",
                            "openvpn3-service-client could not register '"
                            + busname + "' on the D-Bus");
    };


private:
    unsigned int default_log_level = 6; // LogCategory::DEBUG messages
    pid_t start_pid;
    std::string session_token;
    std::string object_path;
    ProcessSignalProducer * procsig;
    BackendClientObject::Ptr be_obj;
    BackendSignals *signal;
};


int client_service(ParsedArgs args)
{
    auto extra = args.GetAllExtraArgs();
    if (extra.size() != 1)
    {
        std::cout << "** ERROR ** Invalid usage: " << args.GetArgv0()
                  << " <session registration token>" << std::endl;
        std::cout << std::endl;
        std::cout << "            This program is not intended to be called manually from the command line" << std::endl;
        return 1;
    }

    // Set a new process session ID, and  do a fork again
    pid_t start_pid = getpid();  // Save the current pid, for logging later on
    if (-1 == setsid())
    {
        std::cerr << "** ERROR ** Failed getting a new process session ID:" << strerror(errno) << std::endl;
        return 3;
    }

    pid_t real_pid = fork();
    if (real_pid == 0)
    {
        std::cout << get_version(args.GetArgv0()) << std::endl;

        BackendClientDBus backend_service(start_pid, G_BUS_TYPE_SYSTEM, std::string(extra[0]));
        if (args.Present("log-level"))
        {
            backend_service.SetDefaultLogLevel(std::atoi(args.GetValue("log-level", 0).c_str()));
        }
        backend_service.Setup();

        // Main loop
        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
        g_unix_signal_add(SIGINT, stop_handler, main_loop);
        g_unix_signal_add(SIGTERM, stop_handler, main_loop);
        g_unix_signal_add(SIGHUP, stop_handler, main_loop);
        backend_service.SetMainLoop(main_loop);
        g_main_loop_run(main_loop);
        usleep(500);
        g_main_loop_unref(main_loop);
        return 0;
    }
    else if (real_pid > 0)
    {
        std::cout << "Re-initiated process from pid " << std::to_string(start_pid)
                  << " to backend process pid " << std::to_string(real_pid)
                  << std::endl;
        return 0;
    }

    std::cerr << "** ERROR ** Failed forking out backend process child" << std::endl;
    return 3;
}


int main(int argc, char **argv)
{
    SingleCommand argparser(argv[0], "OpenVPN 3 VPN Client backend service",
                            client_service);
    argparser.AddVersionOption();
    argparser.AddOption("log-level", "LOG-LEVEL", true,
                        "Sets the default log verbosity level (valid values 0-6, default 4)");

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
