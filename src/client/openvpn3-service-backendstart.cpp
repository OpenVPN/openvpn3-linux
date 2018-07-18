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

#include "config.h"
#include "common/cmdargparser.hpp"
#include "dbus/core.hpp"
#include "log/dbus-log.hpp"
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
    BackendStarterSignals(GDBusConnection *conn, std::string object_path)
            : LogSender(conn, LogGroup::BACKENDSTART,
                        OpenVPN3DBus_interf_backends, object_path)
    {
    }

    virtual ~BackendStarterSignals()
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
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        StatusChange(StatusMajor::SESSION, StatusMinor::PROC_KILLED, msg);
        abort();
    }


    /**
     *  Sends Debug log messages which adds D-Bus message details
     *  related to the message
     *
     * @param busname  D-Bus bus name triggering this log event
     * @param path     D-Bus path to the object triggering this log event
     * @param msg      The log message itself
     */
    void Debug(std::string busname, std::string path, std::string msg)
    {
            std::stringstream debug;
            debug << "pid=" << std::to_string(getpid())
                  << ", busname=" << busname
                  << ", path=" << path
                  << ", message=" << msg;
            LogSender::Debug(debug.str());
    }


    /**
     *  Sends a StatusChange signal with a text message
     *
     * @param major  StatusMajor code of the status change
     * @param minor  StatusMinor code of the status change
     * @param msg    String containing a description of the reason for this
     *               status change
     */
    void StatusChange(const StatusMajor major, const StatusMinor minor, std::string msg)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, msg.c_str());
        Send("StatusChange", params);
    }
};


/**
 * Main service object for starting VPN client processes
 */
class BackendStarterObject : public DBusObject,
                             public BackendStarterSignals
{
public:
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
                         const std::vector<std::string> client_args)
        : DBusObject(objpath),
          BackendStarterSignals(dbuscon, objpath),
          dbuscon(dbuscon),
          client_args(client_args)
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='" << OpenVPN3DBus_interf_backends << "'>"
                          << "        <method name='StartClient'>"
                          << "          <arg type='s' name='token' direction='in'/>"
                          << "          <arg type='u' name='pid' direction='out'/>"
                          << "        </method>"
                          << GetLogIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        Debug(busname, objpath, "BackendStarterObject registered");
    }

    ~BackendStarterObject()
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
        OpenLogFile(filename);
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
            gchar *token;
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
        /*
        std::cout << "[BackendStarterObject] get_property(): "
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
            // Child
            char *args[client_args.size()+2];
            unsigned int i = 0;

            for (const auto& arg : client_args)
            {
                args[i++] = (char *) strdup(arg.c_str());
            }
            args[i++] = token;
            args[i++] = nullptr;

#ifdef DEBUG_OPTIONS
            std::cout << "[openvpn3-service-backend] Command line to be started: ";
            for (unsigned int j = 0; j < i; j++)
            {
                std::cout << args[j] << " ";
            }
            std::cout << std::endl << std::endl;
#endif

            execve(args[0], args, NULL);

            // If execve() succeedes, the line below will not be executed at all.
            // So if we come here, there must be an error.
            std::cerr << "** Error starting " << args[0] << ": " << strerror(errno) << std::endl;
        }
        else if( backend_pid > 0)
        {
            // Parent

            std::stringstream msg;
            // Wait for the child process to exit, as the client process will fork again
            int rc = -1;
            int w = waitpid(backend_pid, &rc, 0);
            if (-1 == w)
            {
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

    BackendStarterDBus(GBusType bus_type, const std::vector<std::string> cliargs)
        : DBus(bus_type,
               OpenVPN3DBus_name_backends,
               OpenVPN3DBus_rootp_backends,
               OpenVPN3DBus_interf_backends),
          mainobj(nullptr),
          procsig(nullptr),
          client_args(cliargs),
          logfile("")
    {
    };


    ~BackendStarterDBus()
    {
        delete mainobj;

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
        mainobj = new BackendStarterObject(GetConnection(), GetBusName(),
                                            GetRootPath(), client_args);
        if (!logfile.empty())
        {
            mainobj->OpenLogFile(logfile);
        }
        mainobj->RegisterObject(GetConnection());

        procsig = new ProcessSignalProducer(GetConnection(),
                                            OpenVPN3DBus_interf_backends,
                                            "BackendStarter");
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
    BackendStarterObject * mainobj;
    ProcessSignalProducer * procsig;
    std::vector<std::string> client_args;
    std::string logfile;
};



int backend_starter(ParsedArgs args)
{
    std::cout << get_version(args.GetArgv0()) << std::endl;

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);


    std::vector<std::string> client_args;
#ifdef DEBUG_OPTIONS
    if (args.Present("run-via"))
    {
        client_args.push_back(args.GetValue("run-via", 0));
    }
    if (args.Present("debugger-arg"))
    {
        for (const auto& a : args.GetAllValues("debugger-arg"))
        {
            client_args.push_back(a);
        }
    }
#endif

    client_args.push_back(std::string(LIBEXEC_PATH) + "/openvpn3-service-client");
#ifdef DEBUG_OPTIONS
    if (args.Present("client-no-fork"))
    {
        client_args.push_back("--no-fork");
    }
    if (args.Present("client-no-setsid"))
    {
        client_args.push_back("--no-setsid");
    }
    if (args.Present("client-signal-broadcast"))
    {
        client_args.push_back("--signal-broadcast");
    }
#endif

    unsigned int idle_wait_sec = 3;
    if (args.Present("idle-exit"))
    {
        idle_wait_sec = std::atoi(args.GetValue("idle-exit", 0).c_str());
    }

    BackendStarterDBus backstart(G_BUS_TYPE_SYSTEM, client_args);
    IdleCheck::Ptr idle_exit;
    if (idle_wait_sec > 0)
    {
        idle_exit.reset(new IdleCheck(main_loop,
                                      std::chrono::seconds(idle_wait_sec)));
        idle_exit->SetPollTime(std::chrono::seconds(10));
        backstart.EnableIdleCheck(idle_exit);
    }
#ifdef DEBUG_OPTIONS
    if (idle_wait_sec > 0)
    {
        std::cout << "Idle exit set to " << idle_wait_sec << " seconds" << std::endl;
    }
    else
    {
        std::cout << "Idle exit is disabled" << std::endl;
    }
#endif
    backstart.Setup();


    if (idle_wait_sec > 0)
    {
        idle_exit->Enable();
    }
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

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
#ifdef DEBUG_OPTIONS
    cmd.AddOption("run-via", 0, "DEBUG_PROGAM", true,
                  "Debug option: Run openvpn3-service-client via provided executable (full path required)");
    cmd.AddOption("debugger-arg", 0, "ARG", true,
                  "Debug option: Argument to pass to the DEBUG_PROGAM");
    cmd.AddOption("client-no-fork", 0,
                  "Debug option: Adds the --no-fork argument to openvpn3-service-client");
    cmd.AddOption("client-no-setsid", 0,
                  "Debug option: Adds the --no-setsid argument to openvpn3-service-client");
    cmd.AddOption("client-signal-broadcast", 0,
                  "Debug option: Adds the --signal-broadcast argument to openvpn3-service-client");
#endif
    cmd.AddOption("idle-exit", "SECONDS", true,
                  "How long to wait before exiting if being idle. "
                  "0 disables it (Default: 10 seconds)");


    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
