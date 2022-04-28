//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <algorithm>
#include <memory>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "dbus/glibutils.hpp"

/**
 *  Basic exception class for LogServiceProxy related errors
 */
class LogServiceProxyException : public std::exception
{
public:
    LogServiceProxyException(const std::string& msg) noexcept
        : message(msg)
    {
    }

    LogServiceProxyException(const std::string& msg,
                             const std::string& dbg) noexcept
        : message(msg), debug(dbg)
    {
    }

    virtual ~LogServiceProxyException() = default;


    virtual const char* what() const noexcept
    {
        return message.c_str();
    }


    virtual std::string debug_details() const noexcept
    {
        return debug;
    }


private:
    std::string message = {};
    std::string debug = {};
};



struct LogSubscriberEntry
{
    LogSubscriberEntry(std::string tag,
                         std::string busname,
                         std::string interface,
                         std::string object_path)
        : tag(tag), busname(busname), interface(interface),
          object_path(object_path)
    {
    }

    std::string tag;
    std::string busname;
    std::string interface;
    std::string object_path;
};

typedef std::vector<LogSubscriberEntry> LogSubscribers;


class LogProxy : protected DBusProxy
{
public:
    using Ptr = std::shared_ptr<LogProxy>;

    LogProxy(GDBusConnection* conn, const std::string& path)
        : DBusProxy(conn,
                    OpenVPN3DBus_name_log,
                    OpenVPN3DBus_interf_log,
                    path)
    {
    }

    ~LogProxy() = default;


    const std::string GetPath() const
    {
        return GetProxyPath();
    }


    const unsigned int GetLogLevel() const
    {
        return GetUIntProperty("log_level");
    }


    void SetLogLevel(const unsigned int loglev)
    {
        SetProperty("log_level", loglev);
    }


    const std::string GetSessionPath() const
    {
        return GetStringProperty("session_path");
    }


    const std::string GetLogTarget() const
    {
        return GetStringProperty("target");
    }


    void Remove()
    {
        (void) Call("Remove", true);
    }
};



/**
 *  Client proxy implementation interacting with a
 *  the net.openvpn.v3.log service
 */
class LogServiceProxy : public DBusProxy
{
public:
    typedef std::shared_ptr<LogServiceProxy> Ptr;

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
        try
        {
            (void) GetServiceVersion();
        }
        catch (DBusProxyAccessDeniedException&)
        {
            // Let this pass - service is available
        }
    }


    /**
     *  Helper function to connect to the Log service and attach this caller to
     *  the service.
     *
     * @param conn        GDBusConnection* to the D-Bus system bus
     * @param interface   D-Bus interface the log service will log from this process
     *
     * @return            Returns a LogServiceProxy object pointer, used to detach
     *                    from the log service when this logging is no longer
     *                    needed.
     */
    static LogServiceProxy::Ptr AttachInterface(GDBusConnection *conn,
                                                const std::string interface)
    {
        LogServiceProxy::Ptr lgs = nullptr;
        try
        {
            lgs.reset(new LogServiceProxy(conn));
            lgs->Attach(interface);
        }
        catch(const DBusException& excp)
        {
            if(lgs)
            {
                lgs.reset();
            }
            throw LogServiceProxyException(
                    "Could not connect to " + OpenVPN3DBus_name_log + " service",
                    excp.what());
        }
        return lgs;
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


    void AssignSession(const std::string& sesspath, const std::string& interf)
    {
        GVariant* empty = Call("AssignSession",
                               g_variant_new("(os)",
                                             sesspath.c_str(),
                                             interf.c_str()),
                               false);
        g_variant_unref(empty);
    }


    LogProxy::Ptr ProxyLogEvents(const std::string& target,
                               const std::string& session_path) const
    {
        GVariant* res = Call("ProxyLogEvents",
                             g_variant_new("(so)",
                                           target.c_str(),
                                           session_path.c_str()));
        if (nullptr == res)
        {
            throw LogServiceProxyException("ProxyLogEvents call failed");
        }

        std::string p = GLibUtils::ExtractValue<std::string>(res, 0);
        LogProxy::Ptr ret;
        ret.reset(new LogProxy(GetConnection(), p));
        g_variant_unref(res);
        return ret;
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


    LogSubscribers GetSubscriberList()
    {
        GVariant *l = Call("GetSubscriberList");
        if (!l)
        {
            THROW_DBUSEXCEPTION("LogServiceProxy",
                                "No subsciber list received");
        }
        GLibUtils::checkParams(__func__, l, "(a(ssss))", 1);

        LogSubscribers list;
        GVariantIter *iter = nullptr;
        g_variant_get(l, "(a(ssss))", &iter);

        GVariant *val = nullptr;
        while ((val = g_variant_iter_next_value(iter)))
        {
            GLibUtils::checkParams(__func__, val, "(ssss)", 4);
            LogSubscriberEntry e(GLibUtils::ExtractValue<std::string>(val, 0),
                                 GLibUtils::ExtractValue<std::string>(val, 1),
                                 GLibUtils::ExtractValue<std::string>(val, 2),
                                 GLibUtils::ExtractValue<std::string>(val, 3));
            list.push_back(e);
            g_variant_unref(val);
        }
        g_variant_iter_free(iter);
        g_variant_unref(l);

        std::sort(list.begin(), list.end(), logsubscribers_sort);
        return list;
    }

private:
    static bool logsubscribers_sort(const LogSubscriberEntry& lhs,
                                    const LogSubscriberEntry& rhs)
    {
        return lhs.busname < rhs.busname;
    }
};
