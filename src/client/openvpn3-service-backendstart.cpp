//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/service.hpp>

#include "build-config.h"
#include "common/cmdargparser.hpp"
#include "dbus/constants.hpp"
#include "dbus/signals/statuschange.hpp"
#include "log/dbus-log.hpp"
#include "log/proxy-log.hpp"
#include "common/utils.hpp"

#ifndef BACKEND_CLIENT_BIN
#define BACKEND_CLIENT_BIN "openvpn3-service-client"
#endif

/**
 * Helper class to tackle signals sent by the backend starter process
 *
 * This mostly just wraps the LogSender class and predfines LogGroup to always
 * be BACKENDSTART.
 */
class BackendStarterSignals : public LogSender
{
  public:
    using Ptr = std::shared_ptr<BackendStarterSignals>;

    /**
     *  Initializes the signaling component
     *
     * @param conn        Existing D-Bus connection to use for sending signals
     * @param log_level   Verbosity level; threshold level for sending signals
     */
    BackendStarterSignals(DBus::Connection::Ptr conn, unsigned int log_level)
        : LogSender(conn,
                    LogGroup::BACKENDSTART,
                    Constants::GenPath("backends"),
                    Constants::GenInterface("backends"))
    {
        SetLogLevel(log_level);
    }

    virtual ~BackendStarterSignals() = default;


    /**
     *  Provides access to the ::Signals::StatusChange object, used to
     *  send the StatusChange D-Bus signals
     *
     * @param sig   ::Signals::StatusChange::Ptr to use
     */
    void ProvideStatusChangeSender(::Signals::StatusChange::Ptr sig)
    {
        sig_statuschg = sig;
    }


    /**
     *  Whenever a FATAL error happens, the process is expected to stop.
     *  The abort() call gets caught by the main loop, which then starts the
     *  proper shutdown process.
     *
     * @param msg  Message to sent to the log subscribers
     */
    void LogFATAL(const std::string &msg)
    {
        Log(Events::Log(log_group, LogCategory::FATAL, msg));
        StatusChange(Events::Status(StatusMajor::SESSION,
                                    StatusMinor::PROC_KILLED,
                                    msg));
        abort();
    }


    /**
     *  Sends a StatusChange signal, based on the content in an
     *  Event::Status object
     *
     * @param event  Event::Status object containing the signal details
     */
    void StatusChange(Events::Status event)
    {
        if (!sig_statuschg)
        {
            return;
        }

        sig_statuschg->Send(event);
    }


  private:
    ::Signals::StatusChange::Ptr sig_statuschg = nullptr;
};



/**
 * Main service object for starting VPN client processes
 */
class BackendStarterHandler : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<BackendStarterHandler>;

    /**
     *  Prepare the service handler object for net.openvpn.v3.backends
     *
     * @param dbuscon_        DBus::Connection::Ptr where the service is hosted
     * @param client_args     Client command line arguments, with the full path
     *                        to the openvpn3-service-client binary
     * @param client_envvars  Additional environment variables set when starting
     *                        the binary
     * @param log_level       Log verbosity level this service uses
     */
    BackendStarterHandler(DBus::Connection::Ptr dbuscon_,
                          const std::vector<std::string> client_args,
                          const std::vector<std::string> client_envvars,
                          unsigned int log_level)
        : DBus::Object::Base(Constants::GenPath("backends"),
                             Constants::GenInterface("backends")),
          dbuscon(dbuscon_),
          creds(DBus::Credentials::Query::Create(dbuscon)),
          client_args(client_args),
          client_envvars(client_envvars),
          process_uid(geteuid())
    {
        DisableIdleDetector(true);

        be_signals = DBus::Signals::Group::Create<BackendStarterSignals>(dbuscon, log_level);
        sig_statuschg = be_signals->CreateSignal<::Signals::StatusChange>();
        be_signals->ProvideStatusChangeSender(sig_statuschg);

        // Target all signals only towards the net.openvpn.v3.log service
        be_signals->AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

        RegisterSignals(be_signals);

        AddProperty("version", version, false);

        auto args = AddMethod("StartClient",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  GVariant *parms = args->GetMethodParameters();
                                  std::string token = glib2::Value::Extract<std::string>(parms, 0);
                                  auto be_pid = static_cast<uint32_t>(this->start_backend_process(token.c_str()));
                                  args->SetMethodReturn(glib2::Value::CreateTupleWrapped(be_pid));
                              });
        args->AddInput("token", glib2::DataType::DBus<std::string>());
        args->AddOutput("pid", glib2::DataType::DBus<uint32_t>());

        be_signals->Debug("BackendStarterObject registered");
    }


    ~BackendStarterHandler()
    {
        be_signals->LogInfo("openvpn3-service-backendstart: Shutting down");
    }


    const bool Authorize(const Authz::Request::Ptr authzreq) override
    {
        // Allow reading properties from anyone
        if (DBus::Object::Operation::PROPERTY_GET == authzreq->operation)
        {
            return true;
        }

        // Restrict access to this service to only come from
        // the same UID as this process is running from; which is defined
        // in the service autostart configuration.
        uid_t caller = creds->GetUID(authzreq->caller);
        be_signals->Debug("Authorize: caller UID:" + std::to_string(caller)
                          + " process UID: " + std::to_string(process_uid));
        return (caller == process_uid);
    }


  private:
    DBus::Connection::Ptr dbuscon{nullptr};
    DBus::Credentials::Query::Ptr creds{nullptr};
    const std::vector<std::string> client_args;
    const std::vector<std::string> client_envvars;
    const uid_t process_uid;
    BackendStarterSignals::Ptr be_signals{nullptr};
    ::Signals::StatusChange::Ptr sig_statuschg{nullptr};
    std::string version{package_version};


    /**
     * Forks out a child thread which starts the openvpn3-service-client
     * process with the provided backend start token.
     *
     * @param token  String containing the start token identifying the session
     *               object this process is tied to.
     * @return Returns the process ID (pid) of the child process.
     */
    pid_t start_backend_process(const char *token)
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
            const char *args[client_args.size() + 2];
            unsigned int i = 0;

            for (const auto &arg : client_args)
            {
                args[i++] = (char *)strdup(arg.c_str());
            }
            args[i++] = token;
            args[i++] = nullptr;

#ifdef OPENVPN_DEBUG
            std::cout << "[openvpn3-service-backend] {" << getpid() << "} "
                      << "Command line to be started: ";
            for (unsigned int j = 0; j < i; j++)
            {
                std::cout << args[j] << " ";
            }
            std::cout << std::endl
                      << std::endl;
#endif

            char **env = {0};
            if (client_envvars.size() > 0)
            {
                env = (char **)std::calloc(client_envvars.size(), sizeof(char *));
                unsigned int idx = 0;
                for (const auto &ev : client_envvars)
                {
                    env[idx] = (char *)ev.c_str();
                    ++idx;
                }
                env[idx] = nullptr;
            }

            execve(args[0], (char **)(args), env);

            // If execve() succeedes, the line below will not be executed
            // at all.  So if we come here, there must be an error.
            std::cerr << "** Error starting " << args[0] << ": "
                      << strerror(errno) << std::endl;
        }
        else if (backend_pid > 0)
        {
            // Parent
            std::stringstream cmdline;
            cmdline << "Command line used {" << getpid() << "}: ";
            for (auto const &c : client_args)
            {
                cmdline << c << " ";
            }
            cmdline << token;
            be_signals->LogVerb2(cmdline.str());

            // Wait for the child process to exit, as the client process will fork again
            int rc = -1;
            int w = waitpid(backend_pid, &rc, 0);
            if (-1 == w)
            {
                std::stringstream msg;
                msg << "Child process (" << token
                    << ") - pid " << backend_pid
                    << " failed to start as expected (exit code: "
                    << std::to_string(rc) << ")";
                be_signals->LogError(msg.str());
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
class BackendStarterSrv : public DBus::Service
{
  public:
    using Ptr = std::shared_ptr<BackendStarterSrv>;

    /**
     * Constructor creating a D-Bus service for the Backend Starter service.
     *
     * @param conn  DBUs::Connection::Ptr where the service is hosted
     */

    BackendStarterSrv(DBus::Connection::Ptr conn,
                      const std::vector<std::string> cliargs,
                      unsigned int log_level)
        : DBus::Service(conn, Constants::GenServiceName("backends")),
          client_args(cliargs),
          log_level(log_level){};

    ~BackendStarterSrv()
    {
    }


    void AddClientEnvVariable(const std::string &envvar)
    {
        client_envvars.push_back(envvar);
    }


    void BusNameAcquired(const std::string &busname) override
    {
        CreateServiceHandler<BackendStarterHandler>(GetConnection(),
                                                    client_args,
                                                    client_envvars,
                                                    log_level);
    };


    void BusNameLost(const std::string &busname) override
    {
        throw DBus::Service::Exception(
            "openvpn3-service-backendstart lost the '"
            + busname + "' registration on the D-Bus");
    };


  private:
    BackendStarterHandler::Ptr mainobj{nullptr};
    std::vector<std::string> client_args{};
    unsigned int log_level{3};
    std::vector<std::string> client_envvars{};
};



int backend_starter(ParsedArgs::Ptr args)
{
    std::cout << get_version(args->GetArgv0()) << std::endl;

    std::vector<std::string> client_args;
#ifdef OPENVPN_DEBUG
    if (args->Present("run-via"))
    {
        client_args.push_back(args->GetValue("run-via", 0));
    }
    if (args->Present("debugger-arg"))
    {
        for (const auto &a : args->GetAllValues("debugger-arg"))
        {
            client_args.push_back(a);
        }
    }
    if (args->Present("client-binary"))
    {
        client_args.push_back(args->GetValue("client-binary", 0));
    }
    else
#endif
    {
        client_args.push_back(LIBEXEC_PATH "/" BACKEND_CLIENT_BIN);
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

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

    auto dbus = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto logsrvprx = LogServiceProxy::AttachInterface(dbus,
                                                      Constants::GenInterface("backends"));
    auto backstart = DBus::Service::Create<BackendStarterSrv>(dbus,
                                                              client_args,
                                                              log_level);
    unsigned int idle_wait_sec = 30;
    if (args->Present("idle-exit"))
    {
        idle_wait_sec = std::atoi(args->GetValue("idle-exit", 0).c_str());
    }

    backstart->PrepareIdleDetector(std::chrono::seconds(idle_wait_sec));

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
        for (const auto &ev : args->GetAllValues("client-setenv"))
        {
            backstart->AddClientEnvVariable(ev);
        }
    }
#endif

    backstart->Run();

    if (logsrvprx)
    {
        std::string interface {
            Constants::GenInterface("backends")
        };
        logsrvprx->Detach(interface);
    }
    return 0;
}



int main(int argc, char **argv)
{
    SingleCommand cmd(argv[0], "OpenVPN 3 VPN Client starter", backend_starter);
    cmd.AddVersionOption();
    cmd.AddOption("log-level",
                  "LOG-LEVEL",
                  true,
                  "Log verbosity level (valid values 0-6, default 3)");
    cmd.AddOption("idle-exit",
                  "SECONDS",
                  true,
                  "How long to wait before exiting if being idle. "
                  "0 disables it (Default: 30 seconds)");
#ifdef OPENVPN_DEBUG
    cmd.AddOption("run-via",
                  0,
                  "DEBUG_PROGAM",
                  true,
                  "Debug option: Run openvpn3-service-client via provided executable (full path required)");
    cmd.AddOption("debugger-arg",
                  0,
                  "ARG",
                  true,
                  "Debug option: Argument to pass to the DEBUG_PROGAM");
    cmd.AddOption("client-no-fork",
                  0,
                  "Debug option: Adds the --no-fork argument to openvpn3-service-client");
    cmd.AddOption("client-no-setsid",
                  0,
                  "Debug option: Adds the --no-setsid argument to openvpn3-service-client");
    cmd.AddOption("client-binary",
                  0,
                  "CLIENT_BINARY",
                  true,
                  "Debug option: Full path to openvpn3-service-client binary; "
                  "default: " LIBEXEC_PATH "/" BACKEND_CLIENT_BIN);
    cmd.AddOption("client-setenv",
                  "ENVVAR=VALUE",
                  true,
                  "Debug option: Sets an environment variable passed to the openvpn3-service-client. "
                  "Can be used multiple times.");
#endif
    cmd.AddOption("client-log-level",
                  "LEVEL",
                  true,
                  "Adds the --log-level LEVEL argument to openvpn3-service-client");
    cmd.AddOption("client-log-file",
                  "FILE",
                  true,
                  "Adds the --log-file FILE argument to openvpn3-service-client");
    cmd.AddOption("client-colour",
                  0,
                  "Adds the --colour argument to openvpn3-service-client");
    cmd.AddOption("client-disable-protect-socket",
                  0,
                  "Adds the --disable-protect argument to openvpn3-service-client");

    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException &excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        return 2;
    }
    catch (CommandException &excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
