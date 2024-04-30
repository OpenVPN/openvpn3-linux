//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   backend-signals.hpp
 *
 * @brief  Helper class for Log, StatusChange and AttentionRequired
 *         sending signals
 */

#pragma once

#include <memory>
#include <sstream>
#include <thread>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/credentials/query.hpp>

#include "dbus/constants.hpp"
#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"
#include "events/attention-req.hpp"

class BackendSignals : public LogSender
{
  public:
    using Ptr = std::shared_ptr<BackendSignals>;

    BackendSignals(DBus::Connection::Ptr conn,
                   LogGroup lgroup,
                   std::string session_token,
                   LogWriter *logwr)
        : LogSender(conn,
                    lgroup,
                    Constants::GenPath("backends/session"),
                    Constants::GenInterface("backends"),
                    true,
                    logwr),
          session_token(session_token)
    {
        SetLogLevel(default_log_level);

        RegisterSignal("AttentionRequired",
                       Events::AttentionReq::SignalDeclaration());

        // Default targets for D-Bus signals are the
        // Session Manager (net.openvpn.v3.sessions) and the
        // Log service (net.openvpn.v3.log).
        auto creds = DBus::Credentials::Query::Create(conn);
        auto sessmgr_busn = creds->GetUniqueBusName(Constants::GenServiceName("sessions"));
        AddTarget(sessmgr_busn);
        AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));

        // Prepare the RegistrationRequest signal; this is only to be sent
        // to the Session Manager (net.openvpn.v3.sessions).  A dedicated signal
        // group is created for this, with only the session manager as the
        // recipient.  This Signal Group is used in RegistrationRequest()
        GroupCreate("sessionmgr");
        GroupAddTarget("sessionmgr", sessmgr_busn);
        RegisterSignal("RegistrationRequest",
                       {{"busname", glib2::DataType::DBus<std::string>()},
                        {"token", glib2::DataType::DBus<std::string>()},
                        {"pid", glib2::DataType::DBus<pid_t>()}});
    }

    [[nodiscard]] static BackendSignals::Ptr Create(DBus::Connection::Ptr conn,
                                                    LogGroup lgroup,
                                                    std::string session_token,
                                                    LogWriter *logwr)
    {
        return BackendSignals::Ptr(new BackendSignals(conn,
                                                      lgroup,
                                                      session_token,
                                                      logwr));
    }

    void RegistrationRequest(const std::string &busname,
                             const std::string &token,
                             const pid_t pid)
    {
        GVariant *param = g_variant_new("(ssi)",
                                        busname.c_str(),
                                        token.c_str(),
                                        pid);
        GroupSendGVariant("sessionmgr", "RegistrationRequest", param);
    }


    void StatusChange(const Events::Status &statusev) override
    {
        status = statusev;
        LogSender::StatusChange(statusev);
    }


    void StatusChange(const StatusMajor maj, const StatusMinor min, const std::string &msg = "")
    {
        status = Events::Status(maj, min, msg);
        LogSender::StatusChange(status);
    }


    void Log(const Events::Log &logev,
             bool duplicate_check = false,
             const std::string &target = "") final
    {
        Events::Log l(logev, session_token);
        LogSender::Log(l, duplicate_check, logger_busname);
    }


    /**
     * Sends a FATAL log messages and kills itself
     *
     * @param Log message to send to the log subscribers
     */
    void LogFATAL(const std::string &msg) override
    {
        Log(Events::Log(log_group, LogCategory::FATAL, msg));
        // This is essentially a glib2 hack, to allow on going signals to
        // be properly sent before we shut down.
        delayed_shutdown.reset(
            new std::thread([]()
                            {
                                sleep(3);
                                kill(getpid(), SIGHUP);
                            }));
    }


    /**
     * Sends an AttentionRequired signal, which tells a front-end that this
     * VPN backend client needs some input or feedback.
     *
     * @param att_type   ClientAttentionType of the attention required
     * @param att_group  ClientAttentionGroup of the attention required
     * @param msg        Simple string message describing what is needed.
     */
    void AttentionReq(const ClientAttentionType att_type,
                      const ClientAttentionGroup att_group,
                      std::string msg)
    {
        GVariant *params = g_variant_new("(uus)",
                                         static_cast<uint32_t>(att_type),
                                         static_cast<uint32_t>(att_group),
                                         msg.c_str());
        SendGVariant("AttentionRequired", params);
    }


    /**
     *  Retrieve the last status message processed
     *
     * @return  Returns a GVariant object containing a key/value
     *          dictionary of the last signal sent
     */
    GVariant *GetLastStatusChange()
    {
        if (status.empty())
        {
            // Nothing have been logged, nothing to report
            Events::Status empty;
            return empty.GetGVariantTuple();
        }
        return status.GetGVariantTuple();
    }


  private:
    const uint32_t default_log_level = 6; // LogCategory::DEBUG
    std::string session_token = {};
    std::string sessionmgr_busname = {};
    std::string logger_busname = {};
    Events::Status status{};
    std::unique_ptr<std::thread> delayed_shutdown;
};
