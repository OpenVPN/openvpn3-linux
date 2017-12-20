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
 * @file   core-client.hpp
 *
 * @brief  This implements the OpenVPN 3 Core library's VPN client object.
 *
 *         It builds upon the ClientAPI::OpenVPNClient class and provides
 *         the glue between the client D-Bus service and the library API.
 */

#ifndef OPENVPN3_CORE_CLIENT
#define OPENVPN3_CORE_CLIENT

#include <iostream>
#include <thread>
#include <mutex>

#include <openvpn/common/platform.hpp>

// don't export core symbols
#define OPENVPN_CORE_API_VISIBILITY_HIDDEN

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

#include "common/core-extensions.hpp"
#include "backend-signals.hpp"
#include "statistics.hpp"

using namespace openvpn;


/**
 *  Core VPN Client implementation of ClientAPI::OpenVPNClient
 */
class CoreVPNClient : public ClientAPI::OpenVPNClient,
                      public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<CoreVPNClient> Ptr;

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
    CoreVPNClient(BackendSignals *signal, RequiresQueue *userinputq)
            : OpenVPNClient::OpenVPNClient(),
              signal(signal),
              userinputq(userinputq),
              failed_signal_sent(false),
              run_status(StatusMinor::CONN_INIT)
    {
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

private:
    std::string dc_cookie;
    unsigned long evntcount = 0;
    BackendSignals *signal;
    RequiresQueue *userinputq;
    std::mutex event_mutex;
    bool failed_signal_sent;
    StatusMinor run_status;


    virtual bool socket_protect(int socket)
    {
            return true;
    }


    /**
     *  Whenever an event occurs within the core library, this method is
     *  invoked as a kind of callback.  The provided information will be
     *  evaluated and sent further as D-Bus signals to the session manager
     *  whenever appropriate.
     *
     * @param ev  A ClientAPI::Event object with the current event.
     */
    virtual void event(const ClientAPI::Event& ev) override
    {
        //std::lock_guard<std::mutex> lock(event_mutex);
        evntcount++;

        std::stringstream entry;
        entry << " EVENT [" << evntcount << "][name=" << ev.name << "]: " << ev.info;
        signal->Debug(entry.str());

        // FIXME: Need to evaluate which other ev.name values should trigger
        //        status change messages

        if ("DYNAMIC_CHALLENGE" == ev.name)
        {
            dc_cookie = ev.info;
            signal->Debug("DYNAMIC_CHALLENGE: |" + dc_cookie + "|");

            ClientAPI::DynamicChallenge dc;
            if (ClientAPI::OpenVPNClient::parse_dynamic_challenge(dc_cookie, dc))
            {
                userinputq->RequireAdd(
                                ClientAttentionType::CREDENTIALS,
                                ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                "dynamic_challenge", dc.challenge,
                                dc.echo == 0);

                // Save the dynamic challenge cookie in the userinputq object.
                // This is due to this object will be wiped after the
                // disconnect, so we can't save any states in this object.
                unsigned int dcrid = userinputq->RequireAdd(
                                       ClientAttentionType::CREDENTIALS,
                                       ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                       "dynamic_challenge_cookie", "",
                                       true);
                userinputq->UpdateEntry(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                        dcrid, dc_cookie);
                signal->AttentionReq(ClientAttentionType::CREDENTIALS,
                                     ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                     dc.challenge);
                run_status = StatusMinor::CFG_REQUIRE_USER;
            }
        }
        else if ("GET_CONFIG" == ev.name)
        {
        }
        else if ("TUN_SETUP_FAILED" == ev.name)
        {
            failed_signal_sent = true;
            signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_FAILED);
            run_status = StatusMinor::CONN_FAILED;
        }
        else if ("CONNECTED" == ev.name)
        {
            signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_CONNECTED);
            run_status = StatusMinor::CONN_CONNECTED;
        }
        else if ("AUTH_FAILED" == ev.name)
        {
            signal->StatusChange(StatusMajor::CONNECTION,
                                 StatusMinor::CONN_AUTH_FAILED,
                                 "Authentication failed");
            run_status = StatusMinor::CONN_AUTH_FAILED;
        }
        else if (!failed_signal_sent && "DISCONNECTED" == ev.name)
        {
            if (run_status != StatusMinor::CONN_AUTH_FAILED)
            {
                signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_DISCONNECTED);
                run_status = StatusMinor::CONN_DISCONNECTED;
            }
        }
    }


    /**
     *  Whenever the core library wants to provide log information, it will
     *  send a ClientAPI::LogInfo object to this method.  This will
     *  essentially just proxy this message to D-Bus as a Log signal.
     *
     * @param log  The ClientAPI::LogInfo object to act upon
     */
    virtual void log(const ClientAPI::LogInfo& log)
    {
        signal->LogVerb1(log.text);
    }


    virtual void external_pki_cert_request(ClientAPI::ExternalPKICertRequest& certreq)
    {
        std::cout << "*** external_pki_cert_request" << std::endl;
        certreq.error = true;
        certreq.errorText = "external_pki_cert_request not implemented";
        // FIXME: This needs to trigger a signal to the front-end client
    }


    virtual void external_pki_sign_request(ClientAPI::ExternalPKISignRequest& signreq)
    {
        std::cout << "*** external_pki_sign_request" << std::endl;
        signreq.error = true;
        signreq.errorText = "external_pki_sign_request not implemented";
        // FIXME: This needs to trigger a signal to the front-end client
    }


    virtual bool pause_on_connection_timeout()
    {
            return false;
    }
};

#endif // OPENVPN3_CORE_CLIENT
