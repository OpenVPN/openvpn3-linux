//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  Heiko Hund <heiko@openvpn.net>
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

#include <exception>
#include <sstream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/service.hpp>

#include "build-config.h"

// This needs to be included before any of the OpenVPN 3 Core
// header files.  This is needed to enable the D-Bus logging
// infrastructure for the Core library
#include "log/core-dbus-logger.hpp"

#include <openvpn/common/base64.hpp>

#include "common/machineid.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"
#include "common/cmdargparser.hpp"
#include "common/platforminfo.hpp"
#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "log/ansicolours.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"
#include "log/proxy-log.hpp"
#include "backend-signals.hpp"

#include "core-client.hpp"

using namespace openvpn;


class ClientException : public DBus::Object::Method::Exception
{
  public:
    ClientException(const std::string &method,
                    const std::string &msg,
                    const std::string &err_domain = "net.openvpn.v3.error.client",
                    GError *gliberr = nullptr)
        : DBus::Object::Method::Exception(msg, gliberr)
    {
        DBus::Exception::error_domain = std::move(err_domain);
    }
};



/**
 *  Class managing a specific VPN client tunnel.  This object has its own
 *  unique D-Bus bus name and object path and is designed to only be
 *  accessible by the user running the openvpn3-service-sessiongmr process.
 *  This session manager is the front-end users access point to this object.
 */
class BackendClientObject : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<BackendClientObject>;

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
    BackendClientObject(DBus::Connection::Ptr conn,
                        std::string bus_name,
                        DBus::Object::Path objpath,
                        std::string session_token_,
                        uint32_t default_log_level,
                        LogWriter *logwr,
                        const bool socket_protect_disabl)
        : DBus::Object::Base(objpath, Constants::GenInterface("backends")),
          dbusconn(conn),
          dbus_creds(DBus::Credentials::Query::Create(conn)),
          session_token(session_token_),
          disabled_socket_protect(socket_protect_disabl)
    {
        signal = DBus::Signals::Group::Create<BackendSignals>(conn,
                                                              LogGroup::CLIENT,
                                                              session_token,
                                                              logwr),
        RegisterSignals(signal);
        CoreLog::Connect(signal);
        signal->SetLogLevel(default_log_level);

        userinputq = RequiresQueue::Create();

        auto regconf = AddMethod("RegistrationConfirmation",
                                 [this](DBus::Object::Method::Arguments::Ptr args)
                                 {
                                     GVariant *ret = this->cb_registration_confirmation(args->GetMethodParameters());
                                     args->SetMethodReturn(ret);
                                 });
        regconf->AddInput("token", "s");
        regconf->AddInput("session_path", "o");
        regconf->AddInput("config_path", "o");
        regconf->AddOutput("config_name", "s");

        auto ping = AddMethod("Ping",
                              [](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  args->SetMethodReturn(glib2::Value::CreateTupleWrapped(true));
                              });
        ping->AddOutput("alive", "b");

        AddMethod("Ready",
                  [self = this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      if (self->userinputq && !self->userinputq->QueueAllDone())
                      {
                          throw ClientException("Ready",
                                                "Missing user credentials",
                                                "net.openvpn.v3.error.ready");
                      }
                      if (self->vpnclient
                          && StatusMinor::SESS_AUTH_URL == self->vpnclient->GetRunStatus())
                      {
                          throw ClientException("Ready",
                                                "Pending web authentication",
                                                "net.openvpn.v3.error.ready");
                      }
                      args->SetMethodReturn(nullptr);
                  });

        AddMethod("Connect",
                  [self = this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      self->cb_connect();
                      args->SetMethodReturn(nullptr);
                  });


        auto pause = AddMethod("Pause",
                               [self = this](DBus::Object::Method::Arguments::Ptr args)
                               {
                                   self->cb_pause(args->GetMethodParameters());
                                   args->SetMethodReturn(nullptr);
                               });
        pause->AddInput("reason", "s");

        AddMethod("Resume",
                  [self = this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      self->cb_resume();
                      args->SetMethodReturn(nullptr);
                  });

        AddMethod("Restart",
                  [self = this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      self->cb_restart(self->vpnclient.get());
                      args->SetMethodReturn(nullptr);
                  });

        AddMethod("Disconnect",
                  [self = this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      self->cb_disconnect(self->vpnclient.get());
                      args->SetMethodReturn(nullptr);
                  });

        AddMethod("ForceShutdown",
                  [this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      // This is an emergency break for this process.  This
                      // kills this process without considering if we are in
                      // an already running state.  This is primarily used to
                      // clean-up stray session objects which is considered dead
                      // by the session manager.

                      this->signal->LogInfo("Forcing shutdown of backend process for "
                                            "token "
                                            + this->session_token);
                      this->signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                                                StatusMinor::CONN_DONE));
                      if (mainloop)
                      {
                          mainloop->Stop();
                      }
                      args->SetMethodReturn(nullptr);
                  });

        userinputq->QueueSetup(this,
                               "UserInputQueueGetTypeGroup",
                               "UserInputQueueFetch",
                               "UserInputQueueCheck",
                               "UserInputProvide");
        auto cb_auth_pending = [this]()
        {
            if (this->vpnclient && this->userinputq
                && this->userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                                ClientAttentionGroup::CHALLENGE_AUTH_PENDING))
            {
                std::string cr_resp = this->userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                                    ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                                                                    "auth_pending");
                this->vpnclient->SendAuthPendingResponse(cr_resp);
                // No further processing is needed, as auth pending replies are sent
                // instantly to the server as a Control Channel message
            }
        };
        userinputq->AddCallback(RequiresQueue::CallbackType::PROVIDE_RESPONSE,
                                cb_auth_pending);

        AddProperty("session_path", session_path, false);


        auto prop_dco_get = [this](const DBus::Object::Property::BySpec &prop)
        {
            return glib2::Value::Create(this->vpnconfig.dco);
        };
        auto prop_dco_set = [this](const DBus::Object::Property::BySpec &prop, GVariant *value)
        {
            try
            {
                this->vpnconfig.dco = glib2::Value::Get<bool>(value);
                this->signal->LogVerb1(std::string("Session Manager change: DCO ")
                                       + (vpnconfig.dco ? "enabled" : "disabled"));
            }
            catch (const std::exception &ex)
            {
                this->signal->LogError(ex.what());
            }
            auto upd = prop.PrepareUpdate();
            upd->AddValue(vpnconfig.dco);
            return upd;
        };
        AddPropertyBySpec("dco", glib2::DataType::DBus<bool>(), prop_dco_get, prop_dco_set);

        auto prop_device_path = [self = this](const DBus::Object::Property::BySpec &prop)
        {
            if (!self->vpnclient)
            {
                return glib2::Value::Create(DBus::Object::Path());
            }
            return glib2::Value::Create(self->vpnclient->netcfg_get_device_path());
        };
        AddPropertyBySpec("device_path",
                          glib2::DataType::DBus<DBus::Object::Path>(),
                          prop_device_path);

        auto prop_device_name = [this](const DBus::Object::Property::BySpec &prop)
        {
            if (!this->vpnclient)
            {
                return glib2::Value::Create(std::string(""));
            }
            return glib2::Value::Create(this->vpnclient->netcfg_get_device_name());
        };
        AddPropertyBySpec("device_name", "s", prop_device_name);

        auto prop_log_level_get = [this](const DBus::Object::Property::BySpec &prop)
        {
            return glib2::Value::Create(this->signal->GetLogLevel());
        };
        auto prop_log_level_set = [this](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            try
            {
                this->signal->SetLogLevel(glib2::Value::Get<uint32_t>(value));
                auto upd = prop.PrepareUpdate();
                upd->AddValue(this->signal->GetLogLevel());
                return upd;
            }
            catch (const DBus::Exception &ex)
            {
                this->signal->LogError(ex.what());
            }
            return nullptr;
        };
        AddPropertyBySpec("log_level",
                          glib2::DataType::DBus<uint32_t>(),
                          prop_log_level_get,
                          prop_log_level_set);

        auto prop_session_name = [this](const DBus::Object::Property::BySpec &prop)
        {
            if (!this->vpnclient)
            {
                return glib2::Value::Create(std::string(""));
            }
            return glib2::Value::Create(this->vpnclient->tun_builder_get_session_name());
        };
        AddPropertyBySpec("session_name", "s", prop_session_name);

        auto prop_statistics = [this](const DBus::Object::Property::BySpec &prop)
        {
            GVariantBuilder *res = glib2::Builder::Create("a{sx}");
            if (this->vpnclient)
            {
                auto vpncl = this->vpnclient.get();
                for (const auto &stat : vpncl->GetStats())
                {
                    g_variant_builder_add(res,
                                          "{sx}",
                                          stat.key.c_str(),
                                          stat.value);
                }
            }
            return glib2::Builder::Finish(res);
        };
        AddPropertyBySpec("statistics", "a{sx}", prop_statistics);

        auto prop_status = [this](const DBus::Object::Property::BySpec &prop)
        {
            return this->signal->GetLastStatusChange();
        };
        AddPropertyBySpec("status", "(uus)", prop_status);

        auto prop_last_log = [this](const DBus::Object::Property::BySpec &prop)
        {
            Events::Log l = this->signal->GetLastLogEvent();
            if (!l.empty())
            {
                l.RemoveToken();
                return l.GetGVariantDict();
            }
            return glib2::Builder::CreateEmpty("a{sv}");
        };
        AddPropertyBySpec("last_log_line", "a{sv}", prop_last_log);

        // Send the registration request to the session manager
        signal->RegistrationRequest(bus_name, session_token, getpid());
        signal->LogVerb1("Initializing VPN client session, token "
                         + session_token);
    }

    ~BackendClientObject()
    {
        if (client_thread && client_thread->joinable())
        {
            try
            {
                client_thread->join();
            }
            catch (...)
            {
                // We don't need these errors if they occurs;
                // we're shutting down anyhow.
            }
        }
    }


    const bool Authorize(const Authz::Request::Ptr authzreq) override
    {
        bool ret = false;
        switch (authzreq->operation)
        {
        case DBus::Object::Operation::PROPERTY_GET:
            {
                // The properties listed in restricted_acl_prop_get
                // are restricted to selected callers - determined by
                // the validate_sender() method.
                //
                // All other properties are readable by anyone
                //
                if (std::find(restricted_acl_prop_get.begin(),
                              restricted_acl_prop_get.end(),
                              authzreq->target)
                    != restricted_acl_prop_get.end())
                {
                    ret = validate_sender(authzreq->caller);
                }
                else
                {
                    ret = true;
                }
            }
            break;

        case DBus::Object::Operation::PROPERTY_SET:
            {
                // The properties listed in restricted_acl_prop_set
                // can only be modified by selected callers - determined by
                // the validate_sender() method.
                //
                // Default is that no properties can be modified by anyone
                //
                if (std::find(restricted_acl_prop_set.begin(),
                              restricted_acl_prop_set.end(),
                              authzreq->target)
                    != restricted_acl_prop_set.end())
                {
                    ret = validate_sender(authzreq->caller);
                }
                else
                {
                    ret = false;
                }
            }
            break;

        case DBus::Object::Operation::METHOD_CALL:
            {
                // Only the session manager is allowed to call methods
                ret = validate_sender(authzreq->caller);
                if (ret && vpnclient)
                {
                    // Ensure a vpnclient object is present only when we are
                    // expected to be in an active connection.

                    switch (vpnclient->GetRunStatus())
                    {
                    case StatusMinor::CFG_REQUIRE_USER: // Requires reconnect
                    case StatusMinor::CONN_DISCONNECTED:
                    case StatusMinor::CONN_FAILED:
                        // When a connection has been torn down,
                        // we need to re-establish the client object
                        vpnclient = nullptr;
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
        case DBus::Object::Operation::NONE:
        default:
            ret = false;
        };
        if (!ret)
        {
            std::ostringstream err;
            err << "Authorization failed: " << authzreq;
            signal->LogError(err.str());
        }
        return ret;
    }


    /**
     *  Provides a reference to the Glib2 main loop object.  This is used
     *  to cleanly shutdown this process when the session manager wants to
     *  shutdown this process via D-Bus.
     *
     * @param ml   GMainLoop pointer to the current main loop thread
     */
    void SetMainLoop(DBus::MainLoop::Ptr ml)
    {
        mainloop = ml;
    }


  private:
    DBus::Connection::Ptr dbusconn{nullptr};
    DBus::Credentials::Query::Ptr dbus_creds{nullptr};
    DBus::MainLoop::Ptr mainloop{nullptr};
    BackendSignals::Ptr signal{nullptr};
    std::string session_token{};
    bool profile_log_level_override = false; ///< Cfg profile has a log-level override
    bool registered = false;
    bool paused = false;
    DBus::Object::Path configpath;
    CoreVPNClient::Ptr vpnclient{nullptr};
    bool disabled_socket_protect;
    std::string dns_scope = "global";
    bool ignore_dns_cfg{false};
    std::shared_ptr<std::thread> client_thread{nullptr};
    ClientAPI::Config vpnconfig{};
    ClientAPI::EvalConfig cfgeval{};
    ClientAPI::ProvideCreds creds{};
    RequiresQueue::Ptr userinputq{nullptr};
    std::mutex connect_guard{};
    DBus::Object::Path session_path = "/__unknown";
    const std::vector<std::string> restricted_acl_prop_get{
        "net.openvpn.v3.backends.statistics",
        "net.openvpn.v3.backends.stats",
        "net.openvpn.v3.backends.device_name",
        "net.openvpn.v3.backends.session_name",
        "net.openvpn.v3.backends.last_log_line"};
    const std::vector<std::string> restricted_acl_prop_set{
        "net.openvpn.v3.backends.log_level",
        "net.openvpn.v3.backends.dco"};


    /**
     *  Verify that the proxy caller (D-Bus client) is the
     *  OpenVPN 3 Session Manager (net.openvpn.v3.sessions).
     *
     * @param sender  String containing the unique bus ID of the sender
     * @return true if the caller is the session manager, otherwise false.
     *
     */
    bool validate_sender(std::string sender)
    {
#ifdef DEBUG_DISABLE_SECURITY_CHECKS
        return true;
#endif
        // Only the session manager is supposed to talk to the
        // the backend VPN client service
        std::string id = dbus_creds->GetUniqueBusName(Constants::GenServiceName("sessions"));
        if (id == sender)
        {
            return true;
        }
        signal->Debug("validate_sender() failed: unique id:" + id + ", sender=" + sender);
        return false;
    }


    /**
     *  This is called by the session manager only, as an
     *  acknowledgement from the session manager that it has
     *  linked this client process to a valid session object which
     *  will be accessible for front-end users.
     *
     *  With this call, we also get the D-Bus object path for the
     *  the VPN configuration profile to use.  This is used when
     *  retrieve the configuration profile from the configuration
     *  manager service through the fetch_configuration() call.
     *
     * @param params
     */
    GVariant *cb_registration_confirmation(GVariant *params)
    {
        if (registered)
        {
            throw ClientException("RegistrationConfirmation",
                                  "Backend service is already registered",
                                  "net.openvpn.v3.error.be-registration");
        }

        glib2::Utils::checkParams(__func__, params, "(soo)", 3);
        auto verify_token = glib2::Value::Extract<std::string>(params, 0);
        session_path = glib2::Value::Extract<DBus::Object::Path>(params, 1);
        configpath = glib2::Value::Extract<DBus::Object::Path>(params, 2);

        registered = (session_token == verify_token);

        signal->Debug("Registration confirmation: "
                      + verify_token + " == "
                      + std::string(session_token) + " => "
                      + (registered ? "true" : "false"));

        if (registered)
        {
            LogServiceProxy::Ptr lgs = LogServiceProxy::Create(dbusconn);
            lgs->AssignSession(session_path, Constants::GenInterface("backends"));

            // Fetch the configuration from the config-manager.
            // Since the configuration may be set up for single-use
            // only, we must keep this config as long as we're running
            std::string config_name = fetch_configuration();
            GVariant *ret = glib2::Value::CreateTupleWrapped(config_name);

            // Sets initial state, which also allows us to early
            // report back back if more data is required to be
            // sent by the front-end interface.
            initialize_client();
            signal->LogVerb2("Assigned session path: " + session_path);
            return ret;
        }
        else
        {
            throw ClientException("RegistrationConfirmation",
                                  "Invalid registration token",
                                  "net.openvpn.v3.error.be-registration");
        }
    }


    void cb_connect()
    {
        // This starts the connection against a VPN server
        if (!registered)
        {
            throw ClientException("Connect",
                                  "Backend service is not initialized");
        }

        // This re-initializes the client object.  If we have already
        // tried to connect but got an AUTH_FAILED, either due to wrong
        // credentials or a dynamic challenge from the server, we
        // need to re-establish the vpnclient object.
        //
        // However, if we have received an on going AUTH_PENDING,
        // the session is already running and we ignore new Connect calls
        // to not interrupt the session.
        //
        if (vpnclient)
        {
            bool can_continue = false;
            uint32_t i = 0;
            do
            {
                switch (vpnclient->GetRunStatus())
                {
                case StatusMinor::CFG_OK:
                case StatusMinor::CFG_REQUIRE_USER:
                case StatusMinor::CONN_INIT:
                    // Only these states are "clean" and the client process is
                    // ready to start a new connection
                    can_continue = true;
                    break;

                case StatusMinor::CONN_DISCONNECTING:
                case StatusMinor::CONN_PAUSING:
                case StatusMinor::CONN_RECONNECTING:
                case StatusMinor::CONN_RESUMING:
                    // The connection is already running but the state is
                    // about to change; wait for a bit and re-check
                    i++;
                    if (i > 5)
                    {
                        // If several attempts was tried, exit to avoid blocking
                        throw ClientException("Connect",
                                              "Session is changing connection state, "
                                              "but times out");
                    }
                    sleep(1);
                    break;

                case StatusMinor::CONN_PAUSED:
                    // If the connection is paused, it need to use the resume
                    // call to re-connect
                    return;

                case StatusMinor::CONN_AUTH_FAILED:
                case StatusMinor::CONN_CONNECTED:
                case StatusMinor::CONN_CONNECTING:
                case StatusMinor::CONN_FAILED:
                case StatusMinor::SESS_AUTH_CHALLENGE:
                case StatusMinor::SESS_AUTH_URL:
                default:
                    // Neither of these statuses should result in retrying
                    // to connect.  SESS_AUTH_CHALLENGE and SESS_AUTH_URL
                    // already has a connection running and no re-connect
                    // is needed.
                    return;
                }
            } while (!can_continue);
        }

        // Don't run more connect calls in parallel
        std::lock_guard<std::mutex> guard(connect_guard);

        try
        {
            initialize_client();
        }
        catch (ClientException &excp)
        {
            signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                                StatusMinor::PROC_KILLED,
                                                excp.GetRawError()));
            signal->LogFATAL("Failed to initialize client: "
                             + std::string(excp.GetRawError()));
            throw ClientException("Connect", excp.GetRawError());
        }

        if (userinputq && !userinputq->QueueAllDone())
        {
            throw ClientException("Connect",
                                  "Required user input not provided");
        }
        signal->LogInfo("Starting connection");
        connect();
    }


    void cb_pause(GVariant *params)
    {
        // Pauses and suspends an on-going and connected VPN tunnel.
        // The reason message provided with this call is sent to the
        // log.

        if (!registered || !vpnclient)
        {
            throw ClientException("Pause",
                                  "Backend service is not initialized");
        }

        if (paused)
        {
            throw ClientException("Pause",
                                  "Connection is already paused");
        }

        glib2::Utils::checkParams(__func__, params, "(s)", 1);
        std::string reason = glib2::Value::Extract<std::string>(params, 0);

        signal->LogInfo("Pausing connection");
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_PAUSING,
                                            "Reason: " + reason));
        vpnclient->pause(reason);
        paused = true;
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_PAUSED));
    }


    void cb_resume()
    {
        // Resumes an already paused VPN session

        if (!registered || !vpnclient)
        {
            throw ClientException("Resume",
                                  "Backend service is not initialized");
        }

        if (!paused)
        {
            throw ClientException("Resume",
                                  "Connection is not paused");
        }

        signal->LogInfo("Resuming connection");
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_RESUMING));
        vpnclient->resume();
        paused = false;
    }


    void cb_restart(CoreVPNClient *vpncli_ptr)
    {
        // Does a complete re-connect for an already running VPN
        // session.  This will reuse all the credentials already
        // gathered.

        if (!registered || !vpncli_ptr)
        {
            std::ostringstream dbg;
            dbg << "variables: registered=" << (registered ? "true" : "false")
                << ", vpnclient=" << vpncli_ptr;
            signal->Debug(dbg.str());
            throw ClientException("Restart",
                                  "Backend service is not initialized");
        }
        signal->LogInfo("Restarting connection");
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_RECONNECTING));
        paused = false;
        vpncli_ptr->reconnect(0);
    }


    void cb_disconnect(CoreVPNClient *vpncli_ptr)
    {
        // Disconnect from the server.  This will also shutdown this
        // process.

        if (!registered)
        {
            throw ClientException("Disconnect",
                                  "Backend service is not initialized");
        }

        signal->LogInfo("Stopping connection");
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_DISCONNECTING));
        vpncli_ptr->stop();
        if (client_thread)
        {
            client_thread->join();
        }
        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                            StatusMinor::CONN_DONE));

        if (mainloop)
        {
            mainloop->Stop();
        }
        else
        {
            kill(getpid(), SIGTERM);
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
                throw ClientException("connect()",
                                      "Session object not properly registered or not initialized");
            }

            if (!userinputq)
            {
                signal->LogCritical("User-Input queue system inaccessible.");
                throw ClientException("connect()",
                                      "userinput queue integration released unexpectedly");
            }

            bool provide_creds = false;
            if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                       ClientAttentionGroup::USER_PASSWORD)
                > 0)
            {
                creds.username = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                         ClientAttentionGroup::USER_PASSWORD,
                                                         "username");
                if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                           ClientAttentionGroup::CHALLENGE_DYNAMIC)
                    == 0)
                {
                    creds.password = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                             ClientAttentionGroup::USER_PASSWORD,
                                                             "password");
                }

                if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                           ClientAttentionGroup::CHALLENGE_STATIC)
                    > 0)
                {
                    creds.response = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                             ClientAttentionGroup::CHALLENGE_STATIC,
                                                             "static_challenge");
                }
                provide_creds = true;
            }

            if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                       ClientAttentionGroup::CHALLENGE_DYNAMIC)
                > 0)
            {
                creds.dynamicChallengeCookie = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                                       ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                                                       "dynamic_challenge_cookie");
                creds.response = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                         ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                                         "dynamic_challenge");
                provide_creds = true;
            }

            if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                       ClientAttentionGroup::HTTP_PROXY_CREDS)
                > 0)
            {
                creds.http_proxy_user = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                                ClientAttentionGroup::HTTP_PROXY_CREDS,
                                                                "http_proxy_user");
                creds.http_proxy_pass = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                                ClientAttentionGroup::HTTP_PROXY_CREDS,
                                                                "http_proxy_pass");
                provide_creds = true;
            }

            if (provide_creds)
            {
                ClientAPI::Status cred_res = vpnclient->provide_creds(creds);
                if (cred_res.error)
                {
                    throw ClientException("connect()",
                                          "Credentials error: " + cred_res.message);
                }

                std::stringstream msg;
                msg << "Username/password provided successfully"
                    << " for '"
                    << (creds.username.empty() ? creds.http_proxy_user : creds.username) << "'";
                signal->LogVerb1(msg.str());
                if (!creds.response.empty())
                {
                    std::stringstream dmsg;
                    dmsg << "Dynamic challenge provided successfully"
                         << " for '" << creds.username << "'";
                    signal->LogVerb1(dmsg.str());
                }
            }

            signal->Debug("Using DNS resolver scope: " + dns_scope);
            vpnclient->set_dns_resolver_scope(dns_scope);

            // Start a new client thread ...
            // ... but first clean up if we have an old thread
            if (client_thread)
            {
                if (client_thread->joinable())
                {
                    client_thread->join();
                }
                client_thread.reset();
            }
            client_thread.reset(new std::thread(
                [vpnconfig = this->vpnconfig, vpnclient = this->vpnclient, signal = this->signal]()
                {
                    asio::detail::signal_blocker sigblock; // Block signals in client thread
                    try
                    {
                        signal->Debug(std::string("[Connect] DCO flag: ")
                                      + (vpnconfig.dco ? "enabled" : "disabled"));
                        signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                                            StatusMinor::CONN_CONNECTING));
                        ClientAPI::Status status = vpnclient->connect();
                        if (status.error)
                        {
                            std::stringstream msg;
                            msg << status.message;
                            if (!status.status.empty())
                            {
                                msg << " {" << status.status << "}";
                            }
                            signal->LogError("Connection failed: " + status.message);
                            signal->Debug("Connection failed: " + msg.str());
                            signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                                                StatusMinor::CONN_FAILED,
                                                                msg.str()));
                        }
                    }
                    catch (openvpn::Exception &excp)
                    {
                        signal->LogFATAL(excp.what());
                    }
                }));
        }
        catch (const DBus::Exception &err)
        {
            signal->StatusChange(Events::Status(StatusMajor::CONNECTION,
                                                StatusMinor::PROC_KILLED,
                                                err.what()));
            signal->LogFATAL("Client thread exception: " + std::string(err.what()));
            throw ClientException("connect()", err.what());
        }
    }


    /**
     *   Initializes a new CoreVPNClient object
     */
    void initialize_client()
    {
        if (vpnconfig.content.empty())
        {
            throw ClientException(__func__,
                                  "No configuration profile has been parsed");
        }

        // Create a new VPN client object, which is handling the
        // tunnel itself.
        vpnclient.reset(new CoreVPNClient(dbusconn, signal, userinputq, session_token));
        vpnclient->disable_socket_protect(disabled_socket_protect);
        vpnclient->disable_dns_config(ignore_dns_cfg);

        if (userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::PK_PASSPHRASE)
            > 0)
        {
            vpnconfig.privateKeyPassword = userinputq->GetResponse(ClientAttentionType::CREDENTIALS,
                                                                   ClientAttentionGroup::PK_PASSPHRASE,
                                                                   "pk_passphrase");
        }

        // Parse the configuration profile to an OptionList to be able
        // to extract certain values from the configuration directly
        OptionList parsed_opts;
        try
        {
            // Basic profile limits
            OptionList::Limits limits("profile is too large",
                                      ProfileParseLimits::MAX_PROFILE_SIZE,
                                      ProfileParseLimits::OPT_OVERHEAD,
                                      ProfileParseLimits::TERM_OVERHEAD,
                                      ProfileParseLimits::MAX_LINE_SIZE,
                                      ProfileParseLimits::MAX_DIRECTIVE_SIZE);

            parsed_opts.parse_from_config(vpnconfig.content, &limits);
            parsed_opts.update_map();
        }
        catch (const std::exception &excp)
        {
            throw ClientException(__func__, "Configuration pre-parsing failed: " + std::string(excp.what()));
        }

        //
        // Check if the configuration contains a client certificate or not
        //
        // The client_cert_present does now only consider file based
        // certificates, but is intended to be set also if certificates is
        // expected to be provided by external PKI
        //
        try
        {
            Option cert = parsed_opts.get("cert");
        }
        catch (...)
        {
            // If this happens, the configuration profile does not
            // contain a client certificate - so we disable it
            vpnconfig.disableClientCert = true;
        }

        // Set the log-level based on the --verb argument from the profile
        // unless the configuration profile has a log-level override.
        // This check is handled here because the overrides are parsed before
        // this initialize_client() method is called.  We do not want the
        // configuration file to override the profile overrides.
        if (!profile_log_level_override)
        {
            try
            {
                const char *verb = parsed_opts.get_c_str("verb", 1, 16);
                if (verb)
                {
                    uint32_t v = std::atoi(verb);
                    if (v > 6)
                    {
                        v = 6;
                    }
                    signal->SetLogLevel(v);
                }
            }
            catch (const LogException &)
            {
                signal->LogCritical("Invalid --verb level in configuration profile");
            }
            catch (...)
            {
                // If verb is not found, we use the default log level
            }
        }

        //  Set a unique host/machine ID
        try
        {
            MachineID machineid;
            machineid.success();
            vpnconfig.hwAddrOverride = machineid.get();
        }
        catch (const MachineIDException &excp)
        {
            signal->LogCritical("Could not set a unique host ID: "
                                + excp.GetError());
        }

        // We need to provide a copy of the vpnconfig object, as vpnclient
        // seems to take ownership
        cfgeval = vpnclient->eval_config(ClientAPI::Config(vpnconfig));
        if (vpnconfig.dco && !cfgeval.dcoCompatible)
        {
            signal->LogError("DCO could not be enabled due to issues in the configuration");
            signal->Debug(cfgeval.dcoIncompatibilityReason);
            vpnconfig.dco = false;
            cfgeval = vpnclient->eval_config(ClientAPI::Config(vpnconfig));
        }

        if (cfgeval.error)
        {
            std::stringstream statusmsg;
            statusmsg << "config_path=" << configpath << ", "
                      << "eval_message='" << cfgeval.message << "'";
            signal->LogError("Failed to parse configuration: " + cfgeval.message);
            signal->StatusChange(
                Events::Status(StatusMajor::CONNECTION,
                               StatusMinor::CFG_ERROR,
                               statusmsg.str()));
            signal->Debug(statusmsg.str());
            vpnclient = nullptr;
            throw ClientException(__func__,
                                  "Configuration parsing failed: " + cfgeval.message);
        }

        if (!vpnconfig.disableClientCert && cfgeval.externalPki)
        {
            std::string errmsg = "Failed to parse configuration: "
                                 "Configuration requires external PKI which is not implemented yet.";
            signal->LogError(errmsg);
            throw ClientException(__func__, errmsg);
        }

        signal->StatusChange(
            Events::Status(StatusMajor::CONNECTION,
                           StatusMinor::CFG_OK,
                           "config_path=" + configpath));

        // Do we need username/password?  Or does this configuration allow the
        // client to log in automatically?
        if (!cfgeval.autologin
            && userinputq->QueueCount(ClientAttentionType::CREDENTIALS,
                                      ClientAttentionGroup::USER_PASSWORD)
                   == 0)
        {
            // TODO: Consider to have --auth-nocache approach as well
            userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::USER_PASSWORD,
                                   "username",
                                   "Auth User name",
                                   false);
            userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::USER_PASSWORD,
                                   "password",
                                   "Auth Password",
                                   true);

            if (!cfgeval.staticChallenge.empty())
            {
                userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                       ClientAttentionGroup::CHALLENGE_STATIC,
                                       "static_challenge",
                                       cfgeval.staticChallenge,
                                       !cfgeval.staticChallengeEcho);
            }

            signal->AttentionReq(ClientAttentionType::CREDENTIALS,
                                 ClientAttentionGroup::USER_PASSWORD,
                                 "Username/password credentials needed");

            signal->StatusChange(
                Events::Status(StatusMajor::CONNECTION,
                               StatusMinor::CFG_REQUIRE_USER,
                               "Username/password credentials needed"));
        }

        if (cfgeval.privateKeyPasswordRequired && vpnconfig.privateKeyPassword.length() == 0)
        {
            userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::PK_PASSPHRASE,
                                   "pk_passphrase",
                                   "Private key passphrase",
                                   true);
            signal->AttentionReq(ClientAttentionType::CREDENTIALS,
                                 ClientAttentionGroup::PK_PASSPHRASE,
                                 "Private key passphrase needed");
            signal->StatusChange(StatusMajor::CONNECTION,
                                 StatusMinor::CFG_REQUIRE_USER,
                                 "Private key passphrase needed to connect");
        }
    }


    /**
     *  Retrieves the VPN configuration profile from the configuration
     *  manager.
     *
     *  The configuration is cached in this object as the configuration might
     *  have the 'single_use' attribute set, which means we can only retrieve
     *  it once from the configuration manager.
     *
     *  @returns On success, the configuration file name will be returned
     */
    std::string fetch_configuration()
    {
        // Retrieve confniguration
        signal->LogVerb2("Retrieving configuration from " + configpath);

        std::string config_name;
        try
        {
            auto cfg_proxy = OpenVPN3ConfigurationProxy(dbusconn,
                                                        configpath);
            config_name = cfg_proxy.GetName();

            // We need to extract the all settings *before* calling
            // GetConfig().  If the configuration is tagged as a single-shot
            // config, we cannot query it for more details after the first
            // GetConfig() call.
            bool dco = cfg_proxy.GetDCO();
            std::vector<OverrideValue> overrides = cfg_proxy.GetOverrides();

            // Parse the configuration
            ProfileMergeFromString pm(cfg_proxy.GetConfig(),
                                      "",
                                      ProfileMerge::FOLLOW_NONE,
                                      ProfileParseLimits::MAX_LINE_SIZE,
                                      ProfileParseLimits::MAX_PROFILE_SIZE);
            vpnconfig.guiVersion = get_guiversion();
            vpnconfig.info = true;
            vpnconfig.content = pm.profile_content();
            vpnconfig.ssoMethods = "openurl,webauth,crtext";
            vpnconfig.dco = dco;

            try
            {
                PlatformInfo platinfo(dbusconn);
                vpnconfig.platformVersion = platinfo.str();
            }
            catch (const std::exception &ex)
            {
                signal->LogError(ex.what());
            }

            set_overrides(overrides);
        }
        catch (std::exception &e)
        {
            // This should normally not happen
            signal->LogFATAL("** EXCEPTION ** openvpn3-service-client/fetch_config():"
                             + std::string(e.what()));
        }
        return config_name;
    }


    void set_overrides(std::vector<OverrideValue> &overrides)
    {
        for (const auto &override : overrides)
        {
            bool valid_override = false;
            if (override.override.key == "server-override")
            {
                vpnconfig.serverOverride = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "port-override")
            {
                vpnconfig.portOverride = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "proto-override")
            {
                vpnconfig.protoOverride = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "ipv6")
            {
                vpnconfig.allowUnusedAddrFamilies = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "log-level")
            {
                signal->SetLogLevel(std::atoi(override.strValue.c_str()));
                profile_log_level_override = true;
                valid_override = true;
            }
            else if (override.override.key == "dns-fallback-google")
            {
                vpnconfig.googleDnsFallback = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "dns-setup-disabled")
            {
                ignore_dns_cfg = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "dns-scope")
            {
                if ("global" == override.strValue
                    || "tunnel" == override.strValue)
                {
                    dns_scope = override.strValue;
                    valid_override = true;
                }
            }
            else if (override.override.key == "dns-sync-lookup")
            {
                vpnconfig.synchronousDnsLookup = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "auth-fail-retry")
            {
                vpnconfig.retryOnAuthFailed = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "allow-compression")
            {
                vpnconfig.compressionMode = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "enable-legacy-algorithms")
            {
                vpnconfig.enableNonPreferredDCAlgorithms = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "tls-version-min")
            {
                vpnconfig.tlsVersionMinOverride = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "tls-cert-profile")
            {
                vpnconfig.tlsCertProfileOverride = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "persist-tun")
            {
                vpnconfig.tunPersist = override.boolValue;
                valid_override = true;
            }
            else if (override.override.key == "proxy-host")
            {
                vpnconfig.proxyHost = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "proxy-port")
            {
                vpnconfig.proxyPort = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "proxy-username")
            {
                vpnconfig.proxyUsername = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "proxy-password")
            {
                vpnconfig.proxyPassword = override.strValue;
                valid_override = true;
            }
            else if (override.override.key == "proxy-auth-cleartext")
            {
                vpnconfig.proxyAllowCleartextAuth = override.boolValue;
                valid_override = true;
            }

            // Add some logging to the overrides which got processed
            if (valid_override)
            {
                std::stringstream msg;

                msg << "Configuration override '"
                    << override.override.key << "' ";

                bool invalid = false;
                switch (override.override.type)
                {
                case OverrideType::string:
                    msg << "set to '" << override.strValue << "'";
                    break;

                case OverrideType::boolean:
                    msg << "set to "
                        << (override.boolValue ? "True" : "False");
                    break;

                case OverrideType::invalid:
                    msg << "contains an invalid value";
                    invalid = true;
                    break;
                }

                if (!invalid)
                {
                    // Valid override values are logged as VERB1 messages
                    signal->LogVerb1(msg.str());
                }
                else
                {
                    // Invalid override values are logged as errors
                    signal->LogError(msg.str());
                }
            }
            else
            {
                // If an override value exists which is not supported,
                // the valid_override will typically be false.  Log this
                // scenario slightly different
                signal->LogError("Unsupported override: "
                                 + override.override.key);
            }
        }
    }
};



class ClientService : public DBus::Service
{
  public:
    ClientService(pid_t start_pid_,
                  DBus::Connection::Ptr dbuscon_,
                  std::string sesstoken,
                  LogWriter *logwr_)
        : DBus::Service(dbuscon_, Constants::GenServiceName("backends.be") + to_string(getpid())),
          dbuscon(dbuscon_),
          start_pid(start_pid_),
          session_token(sesstoken),
          logwr(logwr_)
    {
        logservice = LogServiceProxy::Create(dbuscon);
    };

    ~ClientService() noexcept
    {
        try
        {

            logservice->Detach(Constants::GenInterface("backends"));
            logservice->Detach(Constants::GenInterface("sessions"));
        }
        catch (const std::exception &excp)
        {
            std::cerr << "** ERROR **  Failed closing down D-Bus connection: "
                      << std::string(excp.what());
        }
    }


    /**
     *  Provides a reference to the Glib2 main loop object.  This is used
     *  to cleanly shutdown this process when the session manager wants to
     *  shutdown this process via D-Bus.
     *
     * @param ml   GMainLoop pointer to the current main loop thread
     */
    void SetMainLoop(DBus::MainLoop::Ptr ml)
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
    void SetDefaultLogLevel(uint32_t lvl)
    {
        default_log_level = lvl;
    }


    /**
     *  Sets the flag disabling the ProtectSocket method.  If this is
     *  set to true, any calls to socket_protect ends up as a NOOP with
     *  no errors.  A VERB2 log message will be added to each
     *  socket_protect() call if socket protection has been disabled.
     *
     * @param val Boolean setting the disable flag.  If true, feature is
     *            disabled
     */
    void DisableSocketProtect(bool val)
    {
        disabled_socket_protect = val;
    }


    void BusNameAcquired(const std::string &busname) override
    {
        try
        {
            logservice->Attach(Constants::GenInterface("backends"));
            logservice->Attach(Constants::GenInterface("sessions"));

            DBus::Object::Path object_path = Constants::GenPath("backends/session");
            CreateServiceHandler<BackendClientObject>(dbuscon,
                                                      busname,
                                                      object_path,
                                                      session_token,
                                                      default_log_level,
                                                      logwr,
                                                      disabled_socket_protect);

            signal.reset(new BackendSignals(GetConnection(),
                                            LogGroup::BACKENDPROC,
                                            session_token,
                                            logwr));
            signal->SetLogLevel(default_log_level);
            signal->LogVerb2("Backend client process started as pid "
                             + std::to_string(start_pid)
                             + " daemonized as pid " + std::to_string(getpid()));
            signal->Debug("BackendClientDBus registered on '" + busname
                          + "': " + object_path);
        }
        catch (const DBus::Exception &excp)
        {
            std::cerr << "FATAL ERROR: openvpn3-service-client could not "
                      << "attach to the log service: "
                      << excp.what() << std::endl;
            return; // Throwing an exception here will not be caught/reported
        }
    }

    void BusNameLost(const std::string &busname) override
    {
        throw DBus::Service::Exception(
            "openvpn3-service-client lost the '"
            + busname + "' registration on the D-Bus");
    };


  private:
    DBus::Connection::Ptr dbuscon = nullptr;
    uint32_t default_log_level = 3; // LogCategory::INFO messages
    const pid_t start_pid;
    const std::string session_token;
    LogWriter *logwr;
    BackendClientObject::Ptr be_obj = nullptr;
    bool disabled_socket_protect = false;
    BackendSignals::Ptr signal = nullptr;
    LogServiceProxy::Ptr logservice;
};


/**
 *  Main Backend Client D-Bus service.  This registers this client process
 *  as a separate and unique D-Bus service
 */

void start_client_thread(pid_t start_pid,
                         const std::string argv0,
                         const std::string sesstoken,
                         bool disable_socket_protect,
                         int32_t log_level,
                         LogWriter *logwr)
{
    InitProcess::Init init;
    std::cout << get_version(argv0) << std::endl;

    try
    {
        auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto clientsrv = DBus::Service::Create<ClientService>(start_pid,
                                                              dbuscon,
                                                              sesstoken,
                                                              logwr);

        if (log_level > 0)
        {
            clientsrv->SetDefaultLogLevel(log_level);
        }
        clientsrv->DisableSocketProtect(disable_socket_protect);
        clientsrv->Run();
    }
    catch (const DBus::Exception &excp)
    {
        std::cerr << "** ERROR **  An unrecoveralbe D-Bus error occured"
                  << std::endl
                  << "             " << excp.GetRawError()
                  << std::endl;
    }
    catch (const std::exception &excp)
    {
        std::cerr << "** ERROR **  An unrecoveralbe exception occired"
                  << std::endl
                  << "             " << excp.what()
                  << std::endl;
    }
}



int client_service(ParsedArgs::Ptr args)
{
    auto extra = args->GetAllExtraArgs();
    if (extra.size() != 1)
    {
        std::cout << "** ERROR ** Invalid usage: " << args->GetArgv0()
                  << " <session registration token>" << std::endl;
        std::cout << std::endl;
        std::cout << "            This program is not intended to be called manually from the command line" << std::endl;
        return 1;
    }

    // Open a log destination, if requested
    std::ofstream logfs;
    std::streambuf *logstream;
    std::shared_ptr<std::ostream> logfile;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    if (args->Present("log-file"))
    {
        std::string fname = args->GetValue("log-file", 0);
        if ("stdout:" != fname)
        {
            logfs.open(fname.c_str(), std::ios_base::app);
            logstream = logfs.rdbuf();
        }
        else
        {
            logstream = std::cout.rdbuf();
        }

        logfile.reset(new std::ostream{logstream});
        if (args->Present("colour"))
        {
            colourengine.reset(new ANSIColours());
            logwr.reset(new ColourStreamWriter(*logfile,
                                               colourengine.get()));
        }
        else
        {
            logwr.reset(new StreamLogWriter(*logfile));
        }
    }

    // Save the current pid, for logging later on
    pid_t start_pid = getpid();

    // Get a new process session ID, unless we're debugging
    // and --no-setsid is used.  This can make gdb debugging simpler.
    if (!args->Present("no-setsid") && (-1 == setsid()))
    {
        std::cerr << "** ERROR ** Failed getting a new process session ID:"
                  << strerror(errno) << std::endl;
        return 3;
    }

    int32_t log_level = -1;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

#ifdef OPENVPN_DEBUG
    // When debugging, we might not want to do a fork.
    if (args->Present("no-fork"))
    {
        try
        {
            openvpn::base64_init_static();
            start_client_thread(getpid(),
                                args->GetArgv0(),
                                extra[0],
                                args->Present("disable-protect-socket"),
                                log_level,
                                logwr.get());
            openvpn::base64_uninit_static();
            return 0;
        }
        catch (std::exception &excp)
        {
            std::cout << "FATAL ERROR: " << excp.what() << std::endl;
            return 3;
        }
        // This should really not happen, but if it does - lets log it
        std::cout << "FATAL ERROR: Unexpected error event in no-fork branch"
                  << std::endl;
        return 8;
    }
#endif

    //
    // This is the normal production code branch
    //
    pid_t real_pid = fork();
    if (real_pid == 0)
    {
        try
        {
            openvpn::base64_init_static();
            start_client_thread(start_pid,
                                args->GetArgv0(),
                                extra[0],
                                args->Present("disable-protect-socket"),
                                log_level,
                                logwr.get());
            openvpn::base64_uninit_static();
            return 0;
        }
        catch (std::exception &excp)
        {
            std::cout << "FATAL ERROR: " << excp.what() << std::endl;
            return 3;
        }
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
    SingleCommand argparser(argv[0], "OpenVPN 3 VPN Client backend service", client_service);
    argparser.AddVersionOption();
    argparser.AddOption("log-level",
                        "LOG-LEVEL",
                        true,
                        "Sets the default log verbosity level (valid values 0-6, default 4)");
    argparser.AddOption("log-file",
                        "FILE",
                        true,
                        "Write log data to FILE.  Use 'stdout:' for console logging.");
    argparser.AddOption("colour",
                        0,
                        "Make the log lines colourful");
    argparser.AddOption("disable-protect-socket",
                        0,
                        "Disable the socket protect call on the UDP/TCP socket. "
                        "This is needed on systems not supporting this feature");
#ifdef OPENVPN_DEBUG
    argparser.AddOption("no-fork",
                        0,
                        "Debug option: Do not fork a child to be run in the background.");
    argparser.AddOption("no-setsid",
                        0,
                        "Debug option: Do not not call setsid(3) when forking process.");
#endif

    try
    {
        return argparser.RunCommand(simple_basename(argv[0]), argc, argv);
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
