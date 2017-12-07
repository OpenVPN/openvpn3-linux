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

using namespace openvpn;

class CoreVPNClient : public ClientAPI::OpenVPNClient,
                      public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<CoreVPNClient> Ptr;

    CoreVPNClient(BackendSignals *signal, RequiresQueue *userinputq)
            : OpenVPNClient::OpenVPNClient(),
              signal(signal),
              userinputq(userinputq),
              failed_signal_sent(false),
              run_status(StatusMinor::CONN_INIT)
    {
    }


    bool is_dynamic_challenge() const
    {
        return !dc_cookie.empty();
    }


    std::string dynamic_challenge_cookie()
    {
        return dc_cookie;
    }


    StatusMinor GetRunStatus()
    {
        return run_status;
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
        else if (!failed_signal_sent && "DISCONNECTED" == ev.name)
        {
            signal->StatusChange(StatusMajor::CONNECTION, StatusMinor::CONN_DISCONNECTED);
            run_status = StatusMinor::CONN_DISCONNECTED;
        }
    }


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
