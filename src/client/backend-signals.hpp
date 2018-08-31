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
 * @file   backend-signals.hpp
 *
 * @brief  Helper class for Log, StatusChange and AttentionRequired
 *         sending signals
 */

#ifndef OPENVPN3_DBUS_CLIENT_BACKENDSIGNALS_HPP
#define OPENVPN3_DBUS_CLIENT_BACKENDSIGNALS_HPP

#include <openvpn/common/rc.hpp>

#include "log/logwriter.hpp"

class BackendSignals : public LogSender,
                       public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<BackendSignals> Ptr;
    BackendSignals(GDBusConnection *conn, LogGroup lgroup,
                   std::string object_path, LogWriter *logwr)
        : LogSender(conn, lgroup, OpenVPN3DBus_interf_backends,
                    object_path, logwr)
    {
        SetLogLevel(default_log_level);
    }

    /**
     * Sends a FATAL log messages and kills itself
     *
     * @param Log message to send to the log subscribers
     */
    void LogFATAL(std::string msg)
    {
        Log(LogEvent(log_group, LogCategory::FATAL, msg));
        kill(getpid(), SIGHUP);
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
        Send("StatusChange", status.GetGVariantTuple());
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
        Send("AttentionRequired", params);
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
    StatusEvent status;
};

#endif  // OPENVPN3_DBUS_CLIENT_BACKENDSIGNALS_HPP
