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


class BackendSignals : public LogSender,
                       public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<BackendSignals> Ptr;
    BackendSignals(GDBusConnection *conn, LogGroup lgroup, std::string object_path)
        : LogSender(conn, lgroup, OpenVPN3DBus_interf_backends, object_path)
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
        Log(log_group, LogCategory::FATAL, msg);
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
        last_major = (guint) major;
        last_minor = (guint) minor;
        last_msg = msg;
        GVariant *params = g_variant_new("(uus)",
                                         last_major, last_minor,
                                         last_msg.c_str());
        Send("StatusChange", params);
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
        if( last_msg.empty() && 0 == last_major && 0 == last_minor)
        {
            return NULL;  // Nothing have been logged, nothing to report
        }

        GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (b, "{sv}", "major", g_variant_new_uint32(last_major));
        g_variant_builder_add (b, "{sv}", "minor", g_variant_new_uint32(last_minor));
        g_variant_builder_add (b, "{sv}", "status_message", g_variant_new_string(last_msg.c_str()));

        GVariant *res = g_variant_builder_end(b);
        g_variant_builder_unref(b);
        return res;
    }


private:
    const unsigned int default_log_level = 6; // LogCategory::DEBUG
    guint32 last_major;
    guint32 last_minor;
    std::string last_msg;
};

#endif  // OPENVPN3_DBUS_CLIENT_BACKENDSIGNALS_HPP
