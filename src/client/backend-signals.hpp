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
 * @file   backend-signals.hpp
 *
 * @brief  Helper class for Log, StatusChange and AttentionRequired
 *         sending signals
 */

#pragma once

#include <sstream>
#include <openvpn/common/rc.hpp>

#include "log/dbus-log.hpp"
#include "log/logwriter.hpp"


class BackendSignals : public LogSender,
                       public DBusConnectionCreds,
                       public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<BackendSignals> Ptr;
    BackendSignals(GDBusConnection *conn, LogGroup lgroup,
                   std::string session_token, LogWriter *logwr)
        : LogSender(conn, lgroup, OpenVPN3DBus_interf_backends,
                    OpenVPN3DBus_rootp_backends_session, logwr),
          DBusConnectionCreds(conn),
          session_token(session_token)
    {
        SetLogLevel(default_log_level);
        configure_signal_targets();
    }

    void EnableBroadcast(bool brdcst) noexcept
    {
        broadcast = brdcst;
        configure_signal_targets();
    }

    const std::string GetSessionPath() const
    {
        return get_object_path();
    }

    void SetSessionPath(const std::string& session_path)
    {
        set_object_path(session_path);
    }

    const std::string GetLogIntrospection() override
    {
        return LogEvent::GetIntrospection("Log", true);
    }

    /**
     *  Reimplement LogSender::Log() to prefix all messages with
     *  the session token.
     *
     * @param logev  LogEvent object containing the log message to log.
     */
    void Log(const LogEvent& logev, bool duplicate_check = false, const std::string& target = "") final
    {
        LogEvent l(logev, session_token);
        LogSender::Log(l, duplicate_check, logger_busname);
    }


    /**
     * Sends a FATAL log messages and kills itself
     *
     * @param Log message to send to the log subscribers
     */
    void LogFATAL(std::string msg) override
    {
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        // This is essentially a glib2 hack, to allow on going signals to
        // be properly sent before we shut down.
        delayed_shutdown.reset(new std::thread([]()
                {
                    sleep(3);
                    kill(getpid(), SIGHUP);
                }
        ));
    }

    /**
     * Sends a StatusChange signal
     *
     * @param major  StatusMajor type of the status change
     * @param minor  StatusMinor type of the status change
     * @param msg    String message with more optional details.  Can be "" if
     *               no message is needed.
     */
    void StatusChange(const StatusMajor major, const StatusMinor minor, std::string msg)
    {
        status.major = major;
        status.minor = minor;
        status.message = msg;
        SendTarget(sessionmgr_busname, "StatusChange", status.GetGVariantTuple());
        SendTarget(logger_busname, "StatusChange", status.GetGVariantTuple());
    }

    /**
     * Sends a StatusChange signal, without sending a string message
     *
     * @param major  StatusMajor type of the status change
     * @param minor  StatusMinor type of the status change
     */
    void StatusChange(const StatusMajor major, const StatusMinor minor)
    {
        StatusChange(major, minor, "");
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
        GVariant *params = g_variant_new("(uus)", (guint) att_type, (guint) att_group, msg.c_str());
        SendTarget(sessionmgr_busname, "AttentionRequired", params);
    }

    /**
     *  Retrieve the last status message processed
     *
     * @return  Returns a GVariant Glib2 object containing a key/value
     *          dictionary of the last signal sent
     */
    GVariant * GetLastStatusChange()
    {
        if( status.empty() )
        {
            return NULL;  // Nothing have been logged, nothing to report
        }
        return status.GetGVariantTuple();
    }


private:
    const unsigned int default_log_level = 6; // LogCategory::DEBUG
    bool broadcast = false;
    std::string session_token;
    std::string sessionmgr_busname = {};
    std::string logger_busname = {};
    StatusEvent status;
    std::unique_ptr<std::thread> delayed_shutdown;


    void configure_signal_targets()
    {
        if (!broadcast)
        {
            sessionmgr_busname = GetUniqueBusID(OpenVPN3DBus_name_sessions);
            logger_busname = GetUniqueBusID(OpenVPN3DBus_name_log);
        }
        else
        {
            sessionmgr_busname = {};
            logger_busname = {};
        }

    }
};
