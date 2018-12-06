//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   proxy-log.hpp
 *
 * @brief  D-Bus proxy interface to the net.openvpn.v3.log service
 */

#pragma once

#include <openvpn/common/rc.hpp>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"

/**
 *  Client proxy implementation interacting with a
 *  the net.openvpn.v3.log service
 */
class LogServiceProxy : public DBusProxy,
                        public RC<thread_unsafe_refcount>

{
public:
    typedef RCPtr<LogServiceProxy> Ptr;

    /**
     *  Initialize the LogServiceProxy.
     *
     * @param dbuscon  D-Bus connection to use for the proxy calls
     */
    LogServiceProxy(GDBusConnection * dbuscon)
        : DBusProxy(dbuscon,
                    OpenVPN3DBus_name_log,
                    OpenVPN3DBus_interf_log,
                    OpenVPN3DBus_rootp_log)
    {
        CheckServiceAvail();
        (void) GetServiceVersion();
    }


    /**
     *  Attach this D-Bus connection to the log service.  By doing this
     *  all Log signals sent from this client will be caught by the log
     *  service.
     *
     * @param interf  String containing the D-Bus interface the log service
     *                subscribe to.
     *
     */
    void Attach(const std::string interf)
    {
        CheckServiceAvail();

        // We do this as a synchronous call, to ensure the backend really
        // responded.  Then we just throw away the empty response, to avoid
        // leaking memory.
        GVariant *empty = Call("Attach", g_variant_new("(s)", interf.c_str()), false);
        g_variant_unref(empty);
    }


    /**
     *  Detach this running connection from the log service.  This will
     *  make the log service unsubscribe from the provided interface.
     *
     * @param interf  String containing the D-Bus interface to unsubscribe
     *                from.
     */
    void Detach(const std::string interf)
    {
        Call("Detach", g_variant_new("(s)", interf.c_str()), true);
    }


    /**
     *  Retrieve the number of subscriptions the log service is attached to
     *
     * @return  Returns an unsigned integer with number of subscribers.
     */
    unsigned int GetNumAttached()
    {
        return GetUIntProperty("num_attached");
    }


    /**
     *  Retrieve the current log level (verbosity) of the log service.  This
     *  is a global setting for all subscriptions.
     *
     * @return  Returns an unsigned int of the log level (0-6).
     */
    unsigned int GetLogLevel()
    {
        return GetUIntProperty("log_level");
    }


    /**
     *  Modifies the log level the log service will use for filtering out
     *  log messages.
     *
     * @param loglvl  Unsigned int of the new log level to use.
     */
    void SetLogLevel(unsigned int loglvl)
    {
        SetProperty("log_level", loglvl);
    }


    /**
     *  Will log entries carry a timestamp?
     *
     * @return  Returns true if log events will be prefixed with at
     *          timestamp.
     */
    bool GetTimestampFlag()
    {
        return GetBoolProperty("timestamp");
    }


    /**
     *  Flips the timestamp flag.  By setting this to false, no timestamps
     *  will be added to log entries.  This is useful if log events are
     *  being processed by another log processing which already adds this
     *  information.
     *
     * @param tstamp  Boolean flag wheter to enable (true) or disable (false)
     *                the log timestamp.
     */
    void SetTimestampFlag(bool tstamp)
    {
        SetProperty("timestamp", tstamp);
    }


    /**
     *  Will log D-Bus details be added to the log (meta log lines)?
     *
     * @return  Returns true if D-Bus details are being added to the logs
     */
    bool GetDBusDetailsLogging()
    {
        return GetBoolProperty("log_dbus_details");
    }


    /**
     *  Flips the D-Bus details log flag.  This is more useful for debugging
     *  to better understand who sends the D-Bus Log signals.
     *
     * @param tstamp  Boolean flag wheter to enable (true) or disable (false)
     *                the D-Bus details logging
     */
    void SetDBusDetailsLogging(bool dbus_details)
    {
        SetProperty("log_dbus_details", dbus_details);
    }
};
