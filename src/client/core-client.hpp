//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Heiko Hund <heiko@openvpn.net>
//

/**
 * @file   core-client.hpp
 *
 * @brief  This implements the OpenVPN 3 Core library's VPN client object.
 *
 *         It builds upon the ClientAPI::OpenVPNClient class and provides
 *         the glue between the client D-Bus service and the library API.
 */

#pragma once

#include "build-config.h"

#include <iostream>
#include <string>
#include <thread>
#include <mutex>

// For some odd reason; this file cannot be included after
// openvpn/tun/linux/client/tunnetlink.hpp ... that seems to
// undefine the ConnectionStats type
#include "statistics.hpp"

// don't export core symbols
#define OPENVPN_CORE_API_VISIBILITY_HIDDEN

#include <openvpn/io/io.hpp>
#include <openvpn/common/platform.hpp>
#ifdef ENABLE_OVPNDCO
#include <openvpn/tun/tunmtu.hpp>
#include <openvpn/tun/linux/client/tunnetlink.hpp>
#endif
#include <client/ovpncli.cpp>
#include <openvpn/common/exception.hpp>
#include <openvpn/common/string.hpp>
#include <openvpn/common/signal.hpp>
#include <openvpn/common/file.hpp>
#include <openvpn/common/getopt.hpp>
#include <openvpn/common/getpw.hpp>
#include <openvpn/common/cleanup.hpp>
#include <openvpn/time/timestr.hpp>
#include <openvpn/ssl/peerinfo.hpp>

#include "client/statistics.hpp"
#include "common/core-extensions.hpp"
#include "common/requiresqueue.hpp"
#include "backend-signals.hpp"

#include "core-client-netcfg.hpp"

using namespace openvpn;


class ParseCR_TEXT
{
  public:
    ParseCR_TEXT(const std::string req)
    {
        std::stringstream r(req);
        std::string tok = {};
        bool crtext_found = false;
        bool flags_parsed = false;
        while (std::getline(r, tok, ':'))
        {
            if (!crtext_found && "CR_TEXT" == tok)
            {
                crtext_found = true;
            }
            else if (crtext_found && !flags_parsed)
            {
                flags_parsed = true;
                response = (tok.find("R") != std::string::npos);
                echo = (tok.find("E") == std::string::npos);
            }
            else if (crtext_found && flags_parsed && challenge.empty())
            {
                challenge = tok;
            }
        }

        valid = crtext_found && flags_parsed && !challenge.empty();
    }


    const bool Valid() const noexcept
    {
        return valid;
    }

    std::string challenge = {};
    bool response = false;
    bool echo = false;


  private:
    bool valid = false;
};



#if defined(USE_TUN_BUILDER)
#define CLIENTBASECLASS NetCfgTunBuilder<ClientAPI::OpenVPNClient>
#else
#define CLIENTBASECLASS DummyTunBuilder
class DummyTunBuilder : public ClientAPI::OpenVPNClient
{
  public:
    DummyTunBuilder(DBus::Connection::Ptr dbusconn,
                    BackendSignals::Ptr signals,
                    const std::string &session_token)
    {
    }


    bool socket_protect(int, std::string, bool) override
    {
        return true;
    }


    std::string tun_builder_get_session_name()
    {
        return session_name;
    }


    std::string netcfg_get_device_path()
    {
        return "";
    }


    std::string netcfg_get_device_name()
    {
        return "";
    }


  protected:
    bool disabled_dns_config;
    std::string dns_scope = "global";


  private:
    std::string session_name;
};
#endif

/**
 *  Core VPN Client implementation of ClientAPI::OpenVPNClient
 */
class CoreVPNClient : public CLIENTBASECLASS
{
  public:
    using Ptr = std::shared_ptr<CoreVPNClient>;

    /**
     *  Constructs the CoreVPNClient object
     *
     * @param signal      Pointer to an existing BackendsSignals object,
     *                    which is used to both logging and sending signals
     *                    to the session manager when this objects needs
     *                    some attention.
     * @param userinputq  Pointer to an existing RequiresQueue object which
     *                    will be used to process dynamic challenge
     *                    interactions and more.
     */
    CoreVPNClient(DBus::Connection::Ptr dbusconn,
                  BackendSignals::Ptr signal_,
                  RequiresQueue::Ptr userinputq_,
                  const std::string &session_tok)
        : CLIENTBASECLASS(dbusconn, signal_, session_tok),
          disabled_socket_protect_fd(false),
          userinputq(userinputq_),
          failed_signal_sent(false),
          run_status(StatusMinor::CONN_INIT)
    {
    }


    void disable_socket_protect(bool val)
    {
        disabled_socket_protect_fd = val;
    }


    void set_dns_resolver_scope(const std::string scope)
    {
        dns_scope = scope;
    }


    void disable_dns_config(bool val)
    {
        disabled_dns_config = val;
    }


    /**
     *  Do we have a dynamic challenge?
     *
     * @return  Returns true if we have a dynamic challenge cookie present
     */
    bool is_dynamic_challenge() const
    {
        return !dc_cookie.empty();
    }


    /**
     *   Provides the dynamic challenge cookie which can be decoded and sent
     *   to the front-end user to get even more authentication credentials
     *
     * @return  Returns a string containing the dynamic challenge cookie
     */
    std::string dynamic_challenge_cookie()
    {
        return dc_cookie;
    }


    /**
     *   Retrieve the current status of this object.  Will typically return
     *   StatusMinor::CFG_REQUIRE_USER if more input is needed by the
     *   front-end user or various StatusMinor::CONN_* statuses.
     *
     * @return  Returns a StatusMinor code representing the current state
     */
    StatusMinor GetRunStatus()
    {
        return run_status;
    }


    /**
     *  Retrieves the connection statistics of a running tunnel
     *
     * @return Returns a ConnectionStats (std::vector<ConnectionStatDetails>)
     *         blob which contains all the connection statistics.
     */
    ConnectionStats GetStats()
    {
        const int n = stats_n();
        std::vector<long long> bundle = stats_bundle();

        ConnectionStats stats;
        for (int i = 0; i < n; ++i)
        {
            const long long value = bundle[i];
            if (value)
            {
                ConnectionStatDetails s(stats_name(i), value);
                stats.push_back(s);
            }
        }
        return stats;
    }



    void SendAuthPendingResponse(const std::string &value)
    {
        post_cc_msg("CR_RESPONSE," + base64->encode(value));
    }


  private:
    std::string dc_cookie;
    unsigned long evntcount = 0;
    bool disabled_socket_protect_fd;
    RequiresQueue::Ptr userinputq;
    std::mutex event_mutex;
    bool failed_signal_sent;
    StatusMinor run_status;
    bool initial_connection = true;
    uint32_t auth_pending_timeout = 0;


    bool socket_protect(int socket, std::string remote, bool ipv6) override
    {
        if (disabled_socket_protect_fd)
        {
            socket = -1;
            signals->LogVerb2("Socket Protect with FD has been disabled");
        }
        return CLIENTBASECLASS::socket_protect(socket, remote, ipv6);
    }


    /**
     *  Whenever an event occurs within the core library, this method is
     *  invoked as a kind of callback.  The provided information will be
     *  evaluated and sent further as D-Bus signals to the session manager
     *  whenever appropriate.
     *
     * @param ev  A ClientAPI::Event object with the current event.
     */
    void event(const ClientAPI::Event &ev) override
    {
        evntcount++;

#ifdef DEBUG_CORE_EVENTS
        std::stringstream entry;
        entry << " EVENT [" << evntcount << "][name=" << ev.name << "]: " << ev.info;
        signals->Debug(entry.str());
#endif

        if ("DYNAMIC_CHALLENGE" == ev.name)
        {
            dc_cookie = ev.info;
            signals->Debug("DYNAMIC_CHALLENGE: |" + dc_cookie + "|");

            ClientAPI::DynamicChallenge dc;
            if (ClientAPI::OpenVPNClientHelper::parse_dynamic_challenge(dc_cookie, dc))
            {
                userinputq->RequireAdd(
                    ClientAttentionType::CREDENTIALS,
                    ClientAttentionGroup::CHALLENGE_DYNAMIC,
                    "dynamic_challenge",
                    dc.challenge,
                    dc.echo == 0);

                // Save the dynamic challenge cookie in the userinputq object.
                // This is due to this object will be wiped after the
                // disconnect, so we can't save any states in this object.
                uint32_t dcrid = userinputq->RequireAdd(
                    ClientAttentionType::CREDENTIALS,
                    ClientAttentionGroup::CHALLENGE_DYNAMIC,
                    "dynamic_challenge_cookie",
                    "",
                    true);
                userinputq->UpdateEntry(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                        dcrid,
                                        dc_cookie);
                signals->AttentionReq(ClientAttentionType::CREDENTIALS,
                                      ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                      dc.challenge);
                signals->StatusChange(StatusMajor::CONNECTION,
                                      StatusMinor::CFG_REQUIRE_USER,
                                      "Dynamic Challenge");
                run_status = StatusMinor::CFG_REQUIRE_USER;
            }
        }
        else if ("AUTH_PENDING" == ev.name)
        {
            auth_pending_timeout = 3600; // Default: 1 hour timeout
            std::stringstream evinf;
            evinf << ev.info;
            std::string tok = {};
            bool timeout_found = false;
            while (std::getline(evinf, tok, ' '))
            {
                if (!timeout_found && "timeout" == tok)
                {
                    timeout_found = true;
                }
                else if (timeout_found)
                {
                    auth_pending_timeout = std::atoi(tok.c_str());
                }
            }
            signals->Debug("Auth pending request received, timeout: "
                           + std::string(std::to_string(auth_pending_timeout)));
        }
        else if ("PROXY_NEED_CREDS" == ev.name)
        {
            signals->Debug("PROXY_NEED_CREDS: |" + ev.info + "|");

            userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::HTTP_PROXY_CREDS,
                                   "http_proxy_user",
                                   "HTTP proxy username",
                                   false);
            userinputq->RequireAdd(ClientAttentionType::CREDENTIALS,
                                   ClientAttentionGroup::HTTP_PROXY_CREDS,
                                   "http_proxy_pass",
                                   "HTTP proxy password",
                                   true);
            signals->AttentionReq(ClientAttentionType::CREDENTIALS,
                                  ClientAttentionGroup::HTTP_PROXY_CREDS,
                                  "");
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CFG_REQUIRE_USER,
                                  "HTTP proxy credentials");
            run_status = StatusMinor::CFG_REQUIRE_USER;
        }
        else if ("WARN" == ev.name)
        {
            signals->LogWarn(ev.info);
        }
        else if ("INFO" == ev.name)
        {
            if (string::starts_with(ev.info, "OPEN_URL:"))
            {
                std::string url = ev.info.substr(9);
                open_url(url, "");
            }
            else if (string::starts_with(ev.info, "WEB_AUTH:"))
            {
                std::string extra = ev.info.substr(9);
                size_t flags_end = extra.find(':');
                if (flags_end != std::string::npos)
                {
                    std::string flags = extra.substr(0, flags_end);
                    std::string url = extra.substr(flags_end + 1);
                    open_url(url, flags);
                }
            }
            else if (auth_pending_timeout && string::starts_with(ev.info, "CR_TEXT:"))
            {
                signals->Debug("Parsing CR_TEXT");
                ParseCR_TEXT crtext(ev.info);

                if (crtext.Valid())
                {
                    if (crtext.response)
                    {
                        userinputq->RequireAdd(
                            ClientAttentionType::CREDENTIALS,
                            ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                            "auth_pending",
                            crtext.challenge,
                            crtext.echo);

                        signals->AttentionReq(ClientAttentionType::CREDENTIALS,
                                              ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                                              crtext.challenge);
                        signals->StatusChange(StatusMajor::CONNECTION,
                                              StatusMinor::CFG_REQUIRE_USER, // FIXME: -> CHALLENGE
                                              crtext.challenge);
                        run_status = StatusMinor::SESS_AUTH_CHALLENGE;
                    }
                    else
                    {
                        signals->LogInfo(crtext.challenge);
                    }
                }
                else
                {
                    run_status = StatusMinor::CONN_AUTH_FAILED;
                    signals->LogError("Failed to decode AUTH_PENDING request");
                    signals->Debug("AUTH_PENDING ERROR: name='" + ev.name + "', info='" + ev.info + "'");
                    signals->StatusChange(StatusMajor::CONNECTION,
                                          run_status,
                                          "Failed to decode authentication request from server");
                    failed_signal_sent = true;
                }
            }
            else
            {
                signals->LogInfo(ev.info);
            }
        }
        else if ("COMPRESSION_ENABLED" == ev.name)
        {
            signals->LogCritical(ev.info);
        }
        else if ("GET_CONFIG" == ev.name)
        {
            signals->LogVerb2("Retrieving configuration from server", true);
        }
        else if ("TUN_SETUP_FAILED" == ev.name
                 || "TUN_IFACE_CREATE" == ev.name
                 || "TUN_IFACE_DISABLED" == ev.name)
        {
            failed_signal_sent = true;
            signals->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_FAILED);
            run_status = StatusMinor::CONN_FAILED;
            signals->LogCritical("Failed configuring TUN device (" + ev.name + ")");
        }
        else if ("CONNECTING" == ev.name)
        {
            // Don't log "Connecting" if we're in reconnect mode
            if (StatusMinor::CONN_RECONNECTING != run_status)
            {
                signals->LogInfo("Connecting");
                signals->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_CONNECTING);
                run_status = StatusMinor::CONN_CONNECTING;
            }
        }
        else if ("WAIT" == ev.name)
        {
            signals->LogVerb1("Waiting for server response", true);
        }
        else if ("WAIT_PROXY" == ev.name)
        {
            signals->LogVerb1("Waiting for proxy server response");
        }
        else if ("CONNECTED" == ev.name)
        {
            signals->LogInfo("Connected: " + ev.info);
            signals->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_CONNECTED);
            run_status = StatusMinor::CONN_CONNECTED;
            initial_connection = false;
        }
        else if ("RECONNECTING" == ev.name)
        {
            // Don't send/log the "reconnecting" state on the
            // first initial connection
            if (!initial_connection)
            {
                signals->LogInfo("Reconnecting");
                signals->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_RECONNECTING);
                run_status = StatusMinor::CONN_RECONNECTING;
            }
        }
        else if ("RESOLVE" == ev.name)
        {
            signals->LogVerb2("Resolving", true);
        }
        else if ("AUTH_FAILED" == ev.name)
        {
            signals->LogVerb1("Authentication failed");
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_AUTH_FAILED,
                                  "Authentication failed");
            run_status = StatusMinor::CONN_AUTH_FAILED;
            failed_signal_sent = true;
        }
        else if ("CERT_VERIFY_FAIL" == ev.name)
        {
            signals->LogCritical("Certificate verification failed:" + ev.info);
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_FAILED,
                                  "Certificate verification failed");
            run_status = StatusMinor::CONN_FAILED;
            failed_signal_sent = true;
        }
        else if ("TLS_VERSION_MIN" == ev.name)
        {
            signals->LogCritical("TLS version is requested by server is too low:" + ev.info);
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_FAILED,
                                  "TLS version too low");
            run_status = StatusMinor::CONN_FAILED;
            failed_signal_sent = true;
        }
        else if ("CONNECTION_TIMEOUT" == ev.name)
        {
            signals->LogInfo("Connection timeout");
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_DISCONNECTING,
                                  "Connection timeout");
            run_status = StatusMinor::CONN_DISCONNECTING;
        }
        else if ("INACTIVE_TIMEOUT" == ev.name)
        {
            signals->LogInfo("Connection closing due to inactivity");
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_DISCONNECTING,
                                  "Connection inactivity");
            run_status = StatusMinor::CONN_DISCONNECTING;
        }
        else if ("PROXY_ERROR" == ev.name)
        {
            signals->LogCritical("Proxy connection error:" + ev.info);
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_FAILED,
                                  "Proxy connection error");
            run_status = StatusMinor::CONN_FAILED;
            failed_signal_sent = true;
        }
        else if ("CLIENT_HALT" == ev.name)
        {
            signals->LogCritical("Client Halt: " + ev.info);
            signals->StatusChange(StatusMajor::CONNECTION,
                                  StatusMinor::CONN_FAILED,
                                  "Client disconnected by server");
            run_status = StatusMinor::CONN_FAILED;
        }
        else if (!failed_signal_sent && "DISCONNECTED" == ev.name)
        {
            if (StatusMinor::CONN_AUTH_FAILED != run_status
                && StatusMinor::CFG_REQUIRE_USER != run_status)
            {
                signals->StatusChange(StatusMajor::CONNECTION,
                                      StatusMinor::CONN_DISCONNECTED);
                run_status = StatusMinor::CONN_DISCONNECTED;
                signals->LogInfo("Disconnected");
            }
        }
        else if (ev.fatal)
        {
            std::string msgtag = "[" + ev.name + "] ";
            signals->LogFATAL(msgtag + ev.info);
        }
    }


    void open_url(std::string &url, const std::string &flags)
    {
        // We currently ignore the flags since we do not have an
        // internal webview or proxy implementation
        signals->AttentionReq(ClientAttentionType::CREDENTIALS,
                              ClientAttentionGroup::OPEN_URL,
                              url);
        signals->StatusChange(StatusMajor::SESSION,
                              StatusMinor::SESS_AUTH_URL,
                              url);
        run_status = StatusMinor::SESS_AUTH_URL;
    }


    /**
     *  Whenever the core library wants to provide log information, it will
     *  send a ClientAPI::LogInfo object to this method.  This will
     *  essentially just proxy this message to D-Bus as a Log signal.
     *
     * @param log  The ClientAPI::LogInfo object to act upon
     */
    void log(const ClientAPI::LogInfo &log) override
    {
        // Log events going via log() are to be considered debug information
        signals->Debug(log.text);
    }


    void external_pki_cert_request(ClientAPI::ExternalPKICertRequest &certreq) override
    {
        std::cout << "*** external_pki_cert_request" << std::endl;
        certreq.error = true;
        certreq.errorText = "external_pki_cert_request not implemented";
        // FIXME: This needs to trigger a signal to the front-end client
    }


    void external_pki_sign_request(ClientAPI::ExternalPKISignRequest &signreq) override
    {
        std::cout << "*** external_pki_sign_request" << std::endl;
        signreq.error = true;
        signreq.errorText = "external_pki_sign_request not implemented";
        // FIXME: This needs to trigger a signal to the front-end client
    }


    bool pause_on_connection_timeout() override
    {
        return false;
    }
};
