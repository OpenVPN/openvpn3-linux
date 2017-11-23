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

#include <sstream>

#include "common/core-extensions.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "dbus/core.hpp"
#include "dbus/path.hpp"
#include "log/dbus-log.hpp"

using namespace openvpn;

#define GUI_VERSION_STRING "0.0.1 (Linux)"

class BackendSignals : public LogSender
{
public:

    BackendSignals(GDBusConnection *conn, LogGroup lgroup, std::string object_path)
        : LogSender(conn, lgroup, OpenVPN3DBus_interf_backends, object_path)
    {
    }

    void LogFATAL(std::string msg)
    {
        Log(log_group, LogCategory::FATAL, msg);
        kill(getpid(), SIGHUP);
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

    void AttentionReq(const ClientAttentionType att_type,
                      const ClientAttentionGroup att_group,
                      std::string msg)
    {
        GVariant *params = g_variant_new("(uus)", (guint) att_type, (guint) att_group, msg.c_str());
        Send("AttentionRequired", params);
    }
};

// FIXME: Cleanup ordering --- core client depends on BackendSignals to be defined
#include "core-client.hpp"

class BackendServiceObject : public DBusObject,
                             public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<BackendServiceObject> Ptr;

    BackendServiceObject(GDBusConnection *conn, std::string bus_name,
                         std::string objpath, std::string session_token)
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

    ~BackendServiceObject()
    {
        CoreVPNClient::uninit_process();
    }

    void SetMainLoop(GMainLoop *ml)
    {
        mainloop = ml;
    }

    void callback_method_call(GDBusConnection *conn,
                              const gchar *sender,
                              const gchar *obj_path,
                              const gchar *intf_name,
                              const gchar *meth_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        std::lock_guard<std::mutex> lg(guard);

        try
        {
            signal.Debug("callback_method_call: meth_name=" + std::string(meth_name));

            if (vpnclient)
            {
                switch(vpnclient->GetRunStatus())
                {
                case StatusMinor::CFG_REQUIRE_USER:
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

            if (0 == g_strcmp0(meth_name, "RegistrationConfirmation"))
            {
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
            else if (0 == g_strcmp0(meth_name, "Ping"))
            {
                // The Ping caller is expected to just return true
                g_dbus_method_invocation_return_value(invoc, g_variant_new("(b)", (bool) true));
                return;
            }
            else if (0 == g_strcmp0(meth_name, "Ready"))
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
            else if (0 == g_strcmp0(meth_name, "Connect"))
            {
                if( !registered )
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }

                initialize_client(); // This re-initializes the client object

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
            else if (0 == g_strcmp0(meth_name, "Disconnect"))
            {
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
            else if (0 == g_strcmp0(meth_name, "UserInputQueueGetTypeGroup"))
            {
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
            else if (0 == g_strcmp0(meth_name, "UserInputQueueFetch"))
            {
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
            else if (0 == g_strcmp0(meth_name, "UserInputQueueCheck"))
            {
                userinputq.QueueCheck(invoc, params);
                return; // QueueCheck() have fed invoc with a result already
            }
            else if (0 == g_strcmp0(meth_name, "UserInputProvide"))
            {
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
            else if (0 == g_strcmp0(meth_name, "Pause"))
            {
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
            else if (0 == g_strcmp0(meth_name, "Resume"))
            {
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
            else if (0 == g_strcmp0(meth_name, "Restart"))
            {
                if (!registered || !vpnclient)
                {
                    THROW_DBUSEXCEPTION("BackendServiceObject", "Backend service is not initialized");
                }
                signal.LogInfo("Restarting connection: " + to_string(obj_path));
                signal.StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_RECONNECTING);
                vpnclient->reconnect(0);
            }
            else if (0 == g_strcmp0(meth_name, "ForceShutdown"))
            {
                signal.LogInfo("Forcing shutting down backend process: " + to_string(obj_path));
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
            std::string errmsg = "Failed executing D-Bus call '" + std::string(meth_name) + "': " + excp.what();
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

    GVariant * callback_get_property (GDBusConnection *conn,
                                      const gchar *sender,
                                      const gchar *obj_path,
                                      const gchar *intf_name,
                                      const gchar *property_name,
                                      GError **error)
    {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "(not implemented");
        return NULL;
    }


    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                                   const gchar *sender,
                                                   const gchar *obj_path,
                                                   const gchar *intf_name,
                                                   const gchar *property_name,
                                                   GVariant *value,
                                                   GError **error)
    {
        THROW_DBUSEXCEPTION("BackendServiceObject", "set property not implemented");
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
                signal.LogVerb1("Username/password provided successfully {'" + creds.username + "', '" + creds.password + "'}");
                if (!creds.response.empty())
                {
                    signal.LogVerb1("Dynamic challenge provided successfully {'" + creds.dynamicChallengeCookie + "', '" + creds.response + "'}");
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


    void initialize_client()
    {
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

    void fetch_configuration()
    {
        // Retrieve confniguration
        signal.LogVerb2("Retrieving configuration from " + configpath);

        auto cfg_proxy = new DBusProxy(G_BUS_TYPE_SYSTEM,
                                       OpenVPN3DBus_name_configuration,
                                       OpenVPN3DBus_interf_configuration,
                                       configpath);
        GVariant *res_g = cfg_proxy->Call("Fetch");
        if (NULL == res_g) {
            THROW_DBUSEXCEPTION("BackendServiceObject",
                                "Failed to retrieve the VPN configuration file");
        }
        gchar *cfg = NULL;
        g_variant_get(res_g, "(s)", &cfg);

        auto vpncfgstr = std::string(cfg);
        ProfileMergeFromString pm(vpncfgstr, "",
                                  ProfileMerge::FOLLOW_NONE,
                                  ProfileParseLimits::MAX_LINE_SIZE,
                                  ProfileParseLimits::MAX_PROFILE_SIZE);
        vpnconfig.guiVersion = openvpn::platform_string("oepnvpn3-service-client", GUI_VERSION_STRING);
        vpnconfig.info = true;
        vpnconfig.content = pm.profile_content();
    }
};


class BackendServiceDBus : public DBus
{
public:
    BackendServiceDBus(pid_t start_pid, GBusType bus_type, std::string sesstoken)
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

    ~BackendServiceDBus()
    {
        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
        delete procsig;
        //delete be_obj;
    }

    void SetMainLoop(GMainLoop *ml)
    {
        if (be_obj)
        {
            be_obj->SetMainLoop(ml);
        }
    }

    void callback_bus_acquired()
    {
        // Create a new OpenVPN3 client session object
        object_path = generate_path_uuid(OpenVPN3DBus_rootp_backends_sessions, 'z');
        be_obj.reset(new BackendServiceObject(GetConnection(), GetBusName(), object_path, session_token));
        be_obj->RegisterObject(GetConnection());

        // Setup a signal object of the backend
        signal = new BackendSignals(GetConnection(), LogGroup::BACKENDPROC, object_path);
        signal->LogVerb1("Backend client process started as pid " + std::to_string(start_pid)
                         + " re-initiated as pid " + std::to_string(getpid()));
        signal->LogVerb2("BackendServiceDBus registered on '" + GetBusName()
                       + "': " + object_path);

        procsig = new ProcessSignalProducer(GetConnection(), OpenVPN3DBus_interf_backends,
                                            object_path, "VPN-Client");
        procsig->ProcessChange(StatusMinor::PROC_STARTED);
    }

    void LogFile(std::string logfile)
    {
        signal->OpenLogFile(logfile);
    }

    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    };

    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        THROW_DBUSEXCEPTION("BackendServiceDBus", "Configuration D-Bus name not registered: '" + busname + "'");
    };


private:
    pid_t start_pid;
    std::string session_token;
    std::string object_path;
    ProcessSignalProducer * procsig;
    BackendServiceObject::Ptr be_obj;
    BackendSignals *signal;
};


int main(int argc, char **argv)
{
    if (argc !=2)
    {
        std::cout << "** ERROR ** Invalid usage: " << argv[0] << " <session registration token>" << std::endl;
        std::cout << std::endl;
        std::cout << "            This program is not intended to be called manually from the command line" << std::endl;
        return 1;
    }

    // Set a new process session ID, and  do a fork again
    pid_t start_pid = getpid();
    if (-1 == setsid())
    {
        std::cerr << "** ERROR ** Failed getting a new process session ID:" << strerror(errno) << std::endl;
        return 3;
    }

    pid_t real_pid = fork();
    if (real_pid == 0)
    {
        BackendServiceDBus backend_service(start_pid, G_BUS_TYPE_SYSTEM, std::string(argv[1]));
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
