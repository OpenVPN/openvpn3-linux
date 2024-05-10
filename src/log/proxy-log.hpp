//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   proxy-log.hpp
 *
 * @brief  D-Bus proxy interface to the net.openvpn.v3.log service
 */

#pragma once

#include <algorithm>
#include <memory>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "dbus/constants.hpp"

using namespace DBus;


/**
 *  Basic exception class for LogServiceProxy related errors
 */
class LogServiceProxyException : public std::exception
{
  public:
    LogServiceProxyException(const std::string &msg) noexcept
        : message(msg)
    {
    }

    LogServiceProxyException(const std::string &msg,
                             const std::string &dbg) noexcept
        : message(msg), debug(dbg)
    {
    }

    virtual ~LogServiceProxyException() = default;

    virtual const char *what() const noexcept
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
                       DBus::Object::Path object_path)
        : tag(tag), busname(busname), interface(interface),
          object_path(object_path)
    {
    }

    std::string tag;
    std::string busname;
    std::string interface;
    DBus::Object::Path object_path;
};

typedef std::vector<LogSubscriberEntry> LogSubscribers;



class LogProxy
{
  public:
    using Ptr = std::shared_ptr<LogProxy>;

    [[nodiscard]] static LogProxy::Ptr Create(DBus::Connection::Ptr connection,
                                              const DBus::Object::Path &path)
    {
        return LogProxy::Ptr(new LogProxy(connection, path));
    }


    ~LogProxy() = default;


    const DBus::Object::Path &GetPath() const
    {
        return target->object_path;
    }


    const unsigned int GetLogLevel() const
    {
        return proxy->GetProperty<unsigned int>(target, "log_level");
    }


    void SetLogLevel(const unsigned int loglev)
    {
        proxy->SetProperty(target, "log_level", loglev);
    }


    const DBus::Object::Path GetSessionPath() const
    {
        return proxy->GetProperty<DBus::Object::Path>(target, "session_path");
    }


    const std::string GetLogTarget() const
    {
        return proxy->GetProperty<std::string>(target, "target");
    }


    void Remove()
    {
        proxy->Call(target, "Remove", nullptr, true);
    }

  private:
    Proxy::Client::Ptr proxy{nullptr};
    Proxy::TargetPreset::Ptr target{nullptr};


    LogProxy(DBus::Connection::Ptr connection, const DBus::Object::Path &path)
        : proxy(Proxy::Client::Create(connection, Constants::GenServiceName("log"))),
          target(Proxy::TargetPreset::Create(path, Constants::GenInterface("log")))
    {
    }
};


/**
 *  Client proxy implementation interacting with a
 *  the net.openvpn.v3.log service
 */
class LogServiceProxy
{
  public:
    typedef std::shared_ptr<LogServiceProxy> Ptr;


    enum class Changed : uint16_t
    {
        // clang-format off
        INITALIZED    = 1 << 1,
        LOGLEVEL      = 1 << 2,
        TSTAMP        = 1 << 3,
        DBUS_DETAILS  = 1 << 4,
        LOGTAG_PREFIX = 1 << 5
        // clang-format on
    };


    /**
     *  Initialize the LogServiceProxy.
     *
     * @param dbuscon  D-Bus connection to use for the proxy calls
     */
    LogServiceProxy(DBus::Connection::Ptr dbuscon)
        : connection(dbuscon),
          logservice(Proxy::Client::Create(connection, Constants::GenServiceName("log"))),
          service_qry(Proxy::Utils::DBusServiceQuery::Create(connection)),
          logtarget(Proxy::TargetPreset::Create(Constants::GenPath("log"),
                                                Constants::GenInterface("log")))
    {
        if (!service_qry->CheckServiceAvail(logservice->GetDestination()))
        {
            throw DBus::Exception("LogServiceProxy", "Log service inaccessible");
        }
        try
        {
            auto proxy_qry = Proxy::Utils::Query::Create(logservice);
            (void)proxy_qry->ServiceVersion(logtarget->object_path,
                                            logtarget->interface);
            cache_initial_settings();
        }
        catch (const DBus::Exception &)
        {
            // Let this pass - service is available
        }
    }


    [[nodiscard]] static LogServiceProxy::Ptr Create(DBus::Connection::Ptr dbuscon)
    {
        return LogServiceProxy::Ptr(new LogServiceProxy(dbuscon));
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
    static LogServiceProxy::Ptr AttachInterface(DBus::Connection::Ptr conn,
                                                const std::string interface)
    {
        auto lgs = LogServiceProxy::Create(conn);
        try
        {
            lgs->Attach(interface);
        }
        catch (const DBus::Exception &excp)
        {
            throw LogServiceProxyException(
                "Could not connect to Log service",
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
        if (!service_qry->CheckServiceAvail(Constants::GenServiceName("log")))
        {
            throw LogServiceProxyException("Could not connect to Log service");
        };

        // We do this as a synchronous call, to ensure the backend really
        // responded.  Then we just throw away the empty response, to avoid
        // leaking memory.
        GVariant *empty = logservice->Call(logtarget,
                                           "Attach",
                                           glib2::Value::CreateTupleWrapped(interf));
        g_variant_unref(empty);
    }


    void AssignSession(const DBus::Object::Path &sesspath, const std::string &interf)
    {

        GVariant *empty = logservice->Call(logtarget,
                                           "AssignSession",
                                           g_variant_new("(os)",
                                                         sesspath.c_str(),
                                                         interf.c_str()),
                                           false);
        g_variant_unref(empty);
    }


    LogProxy::Ptr ProxyLogEvents(const std::string &target,
                                 const DBus::Object::Path &session_path) const
    {
        GVariant *res = logservice->Call(logtarget,
                                         "ProxyLogEvents",
                                         g_variant_new("(so)",
                                                       target.c_str(),
                                                       session_path.c_str()));
        if (nullptr == res)
        {
            throw LogServiceProxyException("ProxyLogEvents call failed");
        }

        auto p = glib2::Value::Extract<DBus::Object::Path>(res, 0);
        auto ret = LogProxy::Create(connection, p);
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
        GVariant *r = logservice->Call(logtarget,
                                       "Detach",
                                       glib2::Value::CreateTupleWrapped(interf));
        g_variant_unref(r);
    }


    /**
     *  Retrieve the configuration/state file openvpn-service-logger is
     *  configured to use.
     */
    const std::string GetConfigFile() const
    {
        return logservice->GetProperty<std::string>(logtarget, "config_file");
    }


    /**
     *  Retrieve which logging method is used by the log service
     */
    const std::string GetLogMethod() const
    {
        return logservice->GetProperty<std::string>(logtarget, "log_method");
    }


    /**
     *  Retrieve the number of subscriptions the log service is attached to
     *
     * @return  Returns an unsigned integer with number of subscribers.
     */
    unsigned int GetNumAttached()
    {
        return logservice->GetProperty<uint32_t>(logtarget, "num_attached");
    }


    /**
     *  Retrieve the current log level (verbosity) of the log service.  This
     *  is a global setting for all subscriptions.
     *
     * @return  Returns an unsigned int of the log level (0-6).
     */
    unsigned int GetLogLevel(const bool oldval = false) const
    {
        if (oldval && CheckChange(Changed::LOGLEVEL))
        {
            return old_loglev;
        }
        return logservice->GetProperty<uint32_t>(logtarget, "log_level");
    }


    /**
     *  Modifies the log level the log service will use for filtering out
     *  log messages.
     *
     * @param loglvl  Unsigned int of the new log level to use.
     */
    void SetLogLevel(unsigned int loglvl)
    {
        cache_initial_settings();
        if (!CheckChange(Changed::LOGLEVEL))
        {
            old_loglev = GetLogLevel();
        }

        if (old_loglev == loglvl)
        {
            return;
        }

        logservice->SetProperty(logtarget, "log_level", loglvl);
        flag_change(Changed::LOGLEVEL);
    }


    /**
     *  Will log entries carry a timestamp?
     *
     * @return  Returns true if log events will be prefixed with at
     *          timestamp.
     */
    bool GetTimestampFlag(const bool oldval = false) const
    {
        if (oldval && CheckChange(Changed::TSTAMP))
        {
            return old_tstamp;
        }
        return logservice->GetProperty<bool>(logtarget, "timestamp");
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
        cache_initial_settings();
        if (!CheckChange(Changed::TSTAMP))
        {
            old_tstamp = GetTimestampFlag();
        }

        if (old_tstamp == tstamp)
        {
            return;
        }

        logservice->SetProperty(logtarget, "timestamp", tstamp);
        flag_change(Changed::TSTAMP);
    }


    /**
     *  Will log D-Bus details be added to the log (meta log lines)?
     *
     * @return  Returns true if D-Bus details are being added to the logs
     */
    bool GetDBusDetailsLogging(const bool oldval = false) const
    {
        if (oldval && CheckChange(Changed::DBUS_DETAILS))
        {
            return old_dbusdet;
        }
        return logservice->GetProperty<bool>(logtarget, "log_dbus_details");
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
        cache_initial_settings();
        if (!CheckChange(Changed::DBUS_DETAILS))
        {
            old_dbusdet = GetDBusDetailsLogging();
        }

        if (old_dbusdet == dbus_details)
        {
            return;
        }

        logservice->SetProperty(logtarget, "log_dbus_details", dbus_details);
        flag_change(Changed::DBUS_DETAILS);
    }


    /**
     *  Retrieve the flag controlling if log messages to the system logs
     *  should be prepended with a log tag
     */
    bool GetLogTagPrepend(const bool oldval = false) const
    {
        if (oldval && CheckChange(Changed::LOGTAG_PREFIX))
        {
            return old_logtagp;
        }
        return logservice->GetProperty<bool>(logtarget, "log_prefix_logtag");
    }


    /**
     *  Changes the log tag prefix/prepend setting, which enables or disables
     *  the '{tag:.....'} prefix in log messages.
     *
     *  Not all log writers will respond to this setting.
     *
     * @param logprep  Bool flag whether to enable (true) or disable (false)
     *                 log tag prefixes.
     */
    void SetLogTagPrepend(const bool logprep)
    {
        cache_initial_settings();
        if (!CheckChange(Changed::LOGTAG_PREFIX))
        {
            old_logtagp = GetLogTagPrepend();
        }

        if (old_logtagp == logprep)
        {
            return;
        }

        logservice->SetProperty(logtarget, "log_prefix_logtag", logprep);
        flag_change(Changed::LOGTAG_PREFIX);
    }


    /**
     *  Check if a certain setting has been changed
     *
     * @param c       Changed flag to check
     *
     * @return Returns true if this setting has been changed
     */
    const bool CheckChange(const Changed &c) const
    {
        return (changes & (uint8_t)c);
    }


    LogSubscribers GetSubscriberList()
    {
        GVariant *l = logservice->Call(logtarget, "GetSubscriberList");
        glib2::Utils::checkParams(__func__, l, "(a(ssss))", 1);

        LogSubscribers list;
        GVariantIter *iter = nullptr;
        g_variant_get(l, "(a(ssss))", &iter);

        GVariant *val = nullptr;
        while ((val = g_variant_iter_next_value(iter)))
        {
            glib2::Utils::checkParams(__func__, val, "(ssss)", 4);
            LogSubscriberEntry e(glib2::Value::Extract<std::string>(val, 0),
                                 glib2::Value::Extract<std::string>(val, 1),
                                 glib2::Value::Extract<std::string>(val, 2),
                                 glib2::Value::Extract<DBus::Object::Path>(val, 3));
            list.push_back(e);
            g_variant_unref(val);
        }
        g_variant_iter_free(iter);
        g_variant_unref(l);

        std::sort(list.begin(), list.end(), logsubscribers_sort);
        return list;
    }


  private:
    DBus::Connection::Ptr connection{nullptr};
    DBus::Proxy::Client::Ptr logservice{nullptr};
    DBus::Proxy::Utils::DBusServiceQuery::Ptr service_qry{};
    DBus::Proxy::TargetPreset::Ptr logtarget{nullptr};

    uint8_t changes = 0;
    unsigned int old_loglev = 0;
    bool old_tstamp = false;
    bool old_dbusdet = false;
    bool old_logtagp = false;


    static bool logsubscribers_sort(const LogSubscriberEntry &lhs,
                                    const LogSubscriberEntry &rhs)
    {
        return lhs.busname < rhs.busname;
    }

    void flag_change(const Changed &c)
    {
        changes |= (uint8_t)c;
    }

    void cache_initial_settings()
    {
        if (CheckChange(Changed::INITALIZED))
        {
            return;
        }
        old_loglev = logservice->GetProperty<uint32_t>(logtarget, "log_level");
        old_tstamp = logservice->GetProperty<bool>(logtarget, "timestamp");
        old_dbusdet = logservice->GetProperty<bool>(logtarget, "log_dbus_details");
        old_logtagp = logservice->GetProperty<bool>(logtarget, "log_prefix_logtag");
        flag_change(Changed::INITALIZED);
    }
};
