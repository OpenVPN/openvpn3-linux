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
 * @file   openvpn3-service-backendstart.cpp
 *
 * @brief  Service side implementation of the Backend Starter service
 *         (net.openvpn.v3.backends).
 *
 *         This service starts openvpn3-service-client processes with a
 *         token provided via the StartClient D-Bus method call.  This
 *         service is supposed to be automatically started by D-Bus, with
 *         root privileges.  This ensures the client process this service
 *         starts also runs with the appropriate privileges.
 */

#include <iostream>

#include <openvpn/common/rc.hpp>

#include "config.h"
#include "common/cmdargparser.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "log/dbus-log.hpp"
#include "log/proxy-log.hpp"
#include "common/utils.hpp"

using namespace openvpn;


/**
 * Helper class to tackle signals sent by the backend starter process
 *
 * This mostly just wraps the LogSender class and predfines LogGroup to always
 * be BACKENDSTART.
 */
class BackendStarterSignals : public LogSender
{
public:
    /**
     *  Initializes the signaling component
     *
     * @param conn        Existing D-Bus connection to use for sending signals
     * @param object_path A string with the D-Bus object path signals sent
     *                    should be attached to
     */
    BackendStarterSignals(GDBusConnection *conn, std::string object_path,
                          unsigned int log_level)
            : LogSender(conn, LogGroup::BACKENDSTART,
                        OpenVPN3DBus_interf_backends, object_path)
    {
        SetLogLevel(log_level);
    }

    virtual ~BackendStarterSignals() = default;


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
        StatusChange(StatusEvent(StatusMajor::SESSION,
                                 StatusMinor::PROC_KILLED,
                                 msg));
        abort();
    }
};


/**
 * Main service object for starting VPN client processes
 */
class BackendStarterObject : public DBusObject,
                             public BackendStarterSignals,
                             public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<BackendStarterObject> Ptr;

    /**
     *  Constructor initializing the Backend Starter to be registered on
     *  the D-Bus.
     *
     * @param dbuscon  D-Bus this object is tied to
     * @param busname  D-Bus bus name this service is registered on
     * @param objpath  D-Bus object path to this object
     */
    BackendStarterObject(GDBusConnection *dbuscon, const std::string busname,
                         const std::string objpath,
                         const std::vector<std::string> client_args,
                         const std::vector<std::string> client_envvars,
                         unsigned int log_level,
                         bool signal_broadcast)
        : DBusObject(objpath),
          BackendStarterSignals(dbuscon, objpath, log_level),
          dbuscon(dbuscon),
          client_args(client_args),
          client_envvars(client_envvars)
    {
        if (!signal_broadcast)
        {
            DBusConnectionCreds credsprx(dbuscon);
            AddTargetBusName(credsprx.GetUniqueBusID(OpenVPN3DBus_name_log));
        }

        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_backends << "'>"
                          << "        <method name='StartClient'>"
                          << "          <arg type='s' name='token' direction='in'/>"
                          << "          <arg type='u' name='pid' direction='out'/>"
                          << "        </method>"
                          << "        <property type='s' name='version' access='read'/>"
                          << GetLogIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        Debug("BackendStarterObject registered");
    }

    ~BackendStarterObject()
    {
        LogInfo("Shutting down");
        RemoveObject(dbuscon);
    }


    /**
     *  Callback method called each time a method in the Backend Starter
     *  service is called over the D-Bus.
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
        if ("StartClient" == method_name)
        {
            IdleCheck_UpdateTimestamp();

            // Retrieve the configuration path for the tunnel
            // from the request
            gchar *token = nullptr;
            g_variant_get (params, "(s)", &token);
            pid_t backend_pid = start_backend_process(token);
            if (-1 == backend_pid)
            {
                GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.backend",
                                                              "Backend client process died");
                g_dbus_method_invocation_return_gerror(invoc, err);
                g_error_free(err);
                return;
            }
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(u)", backend_pid));
        }
    };


    /**
     *  Callback which is used each time a Backend Starter object's D-Bus
     *  property is being read.
     *
     *  For the , this method will just return NULL
     *  with an error set in the GError return pointer.  The
     *  BackendStarterObject does not use properties at all.
     *
     * @param conn           D-Bus connection this event occurred on
     * @param sender         D-Bus bus name of the requester
     * @param obj_path       D-Bus object path to the object being requested
     * @param intf_name      D-Bus interface of the property being accessed
     * @param property_name  The property name being accessed
     * @param error          A GLib2 GError object if an error occurs
     *
     * @return  Returns always NULL, as there are no properties in the
     *          BackendStarterObject.
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
     *  Callback method which is used each time the Backend Starter service
     *  D-Bus object property is being modified.
     *
     *  This will always fail with an exception, as there exists no properties
     *  which can be modified in a BackendStarterObject.
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
        THROW_DBUSEXCEPTION("BackendStarterObject",
                            "set property not implemented");
    }


private:
    GDBusConnection *dbuscon;
    const std::vector<std::string> client_args;
    std::vector<std::string> client_envvars;

    /**
     * Forks out a child thread which starts the openvpn3-service-client
     * process with the provided backend start token.
     *
     * @param token  String containing the start token identifying the session
     *               object this process is tied to.
     * @return Returns the process ID (pid) of the child process.
     */
    pid_t start_backend_process(char * token)
    {
        pid_t backend_pid = fork();
        if (0 == backend_pid)
        {
            //  Child process
            //
            //  In this process scope, we do not have any access
            //  to the D-Bus connection.  So we avoid as much logging
            //  as possible here - and only critical things are sent
            //  to stdout, which will be picked up by other logs on the
            //  system
            //
            char *args[client_args.size()+2];
            unsigned int i = 0;

            for (const auto& arg : client_args)
            {
                args[i++] = (char *) strdup(arg.c_str());
            }
            args[i++] = token;
            args[i++] = nullptr;

#ifdef OPENVPN_DEBUG
            std::cout << "[openvpn3-service-backend] Command line to be started: ";
            for (unsigned int j = 0; j < i; j++)
            {
                std::cout << args[j] << " ";
            }
            std::cout << std::endl << std::endl;
#endif

            char** env = {0};
            if (client_envvars.size() > 0)
            {
                env = (char **) std::calloc(client_envvars.size(), sizeof(char *));
                unsigned int idx = 0;
                for (const auto& ev : client_envvars)
                {
                    env[idx] = (char *) ev.c_str();
                    ++idx;
                }
                env[idx] = nullptr;
            }
            else
            {
                env = nullptr;
            }

            execve(args[0], args, env);

            // If execve() succeedes, the line below will not be executed
            // at all.  So if we come here, there must be an error.
            std::cerr << "** Error starting " << args[0] << ": "
                      << strerror(errno) << std::endl;
        }
        else if( backend_pid > 0)
        {
            // Parent
            std::stringstream cmdline;
            cmdline << "Command line used: ";
            for (auto const& c : client_args)
            {
                cmdline << c << " ";
            }
            cmdline << token;
            LogVerb2(cmdline.str());

            // Wait for the child process to exit, as the client process will fork again
            int rc = -1;
            int w = waitpid(backend_pid, &rc, 0);
            if (-1 == w)
            {
                std::stringstream msg;
                msg << "Child process ("  << token
                    << ") - pid " << backend_pid
                    << " failed to start as expected (exit code: "
                    << std::to_string(rc) << ")";
                LogError(msg.str());
                return -1;
            }
            return backend_pid;
        }
        throw std::runtime_error("Failed to fork() backend client process");
    }
};


/**
 * Main D-Bus service implementation of the Backend Starter service
 */
class BackendStarterDBus : public DBus
{
public:
    /**
     * Constructor creating a D-Bus service for the Backend Starter service.
     *
     * @param bus_type  GBusType, which defines if this service should be
     *                  registered on the system or session bus.
     */

    BackendStarterDBus(GDBusConnection *conn,
                       const std::vector<std::string> cliargs,
                       unsigned int log_level,
                       bool signal_broadcast)
        : DBus(conn,
               OpenVPN3DBus_name_backends,
               OpenVPN3DBus_rootp_backends,
               OpenVPN3DBus_interf_backends),
          mainobj(nullptr),
          log_level(log_level),
          signal_broadcast(signal_broadcast),
          procsig(nullptr),
          client_args(cliargs)
    {
        procsig.reset(new ProcessSignalProducer(conn,
                                                OpenVPN3DBus_interf_backends,
                                                "BackendStarter"));
    };


    ~BackendStarterDBus()
    {
        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
    }


    void AddClientEnvVariable(const std::string& envvar)
    {
        client_envvars.push_back(envvar);
    }


    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired()
    {
        mainobj.reset(new BackendStarterObject(GetConnection(), GetBusName(),
                                               GetRootPath(), client_args,
                                               client_envvars,
                                               log_level, signal_broadcast));
        mainobj->RegisterObject(GetConnection());

        procsig->ProcessChange(StatusMinor::PROC_STARTED);

        if (idle_checker)
        {
            mainobj->IdleCheck_Register(idle_checker);
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
        THROW_DBUSEXCEPTION("BackendStarterDBus",
                            "openvpn3-service-backendstart could not register '"
                            + busname + "' on the D-Bus");
    };

private:
    BackendStarterObject::Ptr mainobj;
    unsigned int log_level = 3;
    bool signal_broadcast = true;
    ProcessSignalProducer::Ptr procsig;
    std::vector<std::string> client_args;
    std::vector<std::string> client_envvars;
};



int backend_starter(ParsedArgs::Ptr args)
{
    std::cout << get_version(args->GetArgv0()) << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);


    std::vector<std::string> client_args;
#ifdef OPENVPN_DEBUG
    if (args->Present("run-via"))
    {
        client_args.push_back(args->GetValue("run-via", 0));
    }
    if (args->Present("debugger-arg"))
    {
        for (const auto& a : args->GetAllValues("debugger-arg"))
        {
            client_args.push_back(a);
        }
    }
    if (args->Present("client-path"))
    {
        client_args.push_back(args->GetValue("client-path", 0));
    }
    else
#endif
    {
        client_args.push_back(std::string(LIBEXEC_PATH) + "/openvpn3-service-client");
    }
#ifdef OPENVPN_DEBUG
    if (args->Present("client-no-fork"))
    {
        client_args.push_back("--no-fork");
    }
    if (args->Present("client-no-setsid"))
    {
        client_args.push_back("--no-setsid");
    }

#endif
    if (args->Present("client-log-level"))
    {
        client_args.push_back("--log-level");
        client_args.push_back(args->GetValue("client-log-level", 0));
    }
    if (args->Present("client-log-file"))
    {
        client_args.push_back("--log-file");
        client_args.push_back(args->GetValue("client-log-file", 0));
    }
    if (args->Present("client-colour"))
    {
        client_args.push_back("--colour");
    }
    if (args->Present("client-disable-protect-socket"))
    {
        client_args.push_back("--disable-protect-socket");
    }
    if (args->Present("client-signal-broadcast"))
    {
        client_args.push_back("--signal-broadcast");
    }

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

    unsigned int idle_wait_sec = 3;
    if (args->Present("idle-exit"))
    {
        idle_wait_sec = std::atoi(args->GetValue("idle-exit", 0).c_str());
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    bool signal_broadcast = args->Present("signal-broadcast");
    LogServiceProxy::Ptr logsrvprx = nullptr;
    if (!signal_broadcast)
    {
        logsrvprx = LogServiceProxy::AttachInterface(dbus.GetConnection(),
                                                     OpenVPN3DBus_interf_backends);
    }

    BackendStarterDBus backstart(dbus.GetConnection(), client_args,
                                 log_level, signal_broadcast);

    IdleCheck::Ptr idle_exit;
    if (idle_wait_sec > 0)
    {
        idle_exit.reset(new IdleCheck(main_loop,
                                      std::chrono::seconds(idle_wait_sec)));
        backstart.EnableIdleCheck(idle_exit);
    }
#ifdef OPENVPN_DEBUG
    if (idle_wait_sec > 0)
    {
        std::cout << "Idle exit set to " << idle_wait_sec << " seconds" << std::endl;
    }
    else
    {
        std::cout << "Idle exit is disabled" << std::endl;
    }

    if (args->Present("client-setenv"))
    {
        for (const auto& ev : args->GetAllValues("client-setenv"))
        {
            backstart.AddClientEnvVariable(ev);
        }
    }

#endif
    backstart.Setup();


    if (idle_wait_sec > 0)
    {
        idle_exit->Enable();
    }
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    if (logsrvprx)
    {
        logsrvprx->Detach(OpenVPN3DBus_interf_backends);
    }

    if (idle_wait_sec > 0)
    {
        idle_exit->Disable();
        idle_exit->Join();
    }

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand cmd(argv[0], "OpenVPN 3 VPN Client starter",
                             backend_starter);
    cmd.AddVersionOption();
    cmd.AddOption("log-level", "LOG-LEVEL", true,
                  "Log verbosity level (valid values 0-6, default 3)");
    cmd.AddOption("signal-broadcast", 0,
                  "Broadcast all D-Bus signals from openvpn3-service-backend instead of targeted unicast");
    cmd.AddOption("idle-exit", "SECONDS", true,
                  "How long to wait before exiting if being idle. "
                  "0 disables it (Default: 10 seconds)");
#ifdef OPENVPN_DEBUG
    cmd.AddOption("run-via", 0, "DEBUG_PROGAM", true,
                  "Debug option: Run openvpn3-service-client via provided executable (full path required)");
    cmd.AddOption("debugger-arg", 0, "ARG", true,
                  "Debug option: Argument to pass to the DEBUG_PROGAM");
    cmd.AddOption("client-no-fork", 0,
                  "Debug option: Adds the --no-fork argument to openvpn3-service-client");
    cmd.AddOption("client-no-setsid", 0,
                  "Debug option: Adds the --no-setsid argument to openvpn3-service-client");
    cmd.AddOption("client-path", 0, "CLIENT-PATH", true,
                  "Debug option: Path to openvpn3-service-client binary");
    cmd.AddOption("client-setenv", "ENVVAR=VALUE", true,
                  "Debug option: Sets an environment variable passed to the openvpn3-service-client. "
                  "Can be used multiple times.");
#endif
    cmd.AddOption("client-log-level", "LEVEL", true,
                  "Adds the --log-level LEVEL argument to openvpn3-service-client");
    cmd.AddOption("client-log-file", "FILE", true,
                  "Adds the --log-file FILE argument to openvpn3-service-client");
    cmd.AddOption("client-colour", 0,
                  "Adds the --colour argument to openvpn3-service-client");
    cmd.AddOption("client-disable-protect-socket", 0,
                  "Adds the --disable-protect argument to openvpn3-service-client");
    cmd.AddOption("client-signal-broadcast", 0,
                  "Debug option: Adds the --signal-broadcast argument to openvpn3-service-client");

    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException& excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        return 2;
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
