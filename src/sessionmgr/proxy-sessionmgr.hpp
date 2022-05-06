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
 * @file   proxy-sessionmgr.hpp
 *
 * @brief  Provides a C++ object implementation of a D-Bus session manger
 *         and session object.  This proxy class will perform the D-Bus
 *         calls and provide the results as native C++ data types.
 */


#pragma once

#include <iostream>
#include <memory>

#include "dbus/core.hpp"
#include "dbus/requiresqueue-proxy.hpp"
#include "client/statistics.hpp"
#include "client/statusevent.hpp"
#include "log/log-helpers.hpp"
#include "log/dbus-log.hpp"

using namespace openvpn;



/**
 * This exception is thrown when the OpenVPN3SessionProxy::Ready() call
 * indicates the VPN backend client needs more information from the
 * frontend process.
 */
class ReadyException : public std::exception
{
public:
    ReadyException(const std::string& err) noexcept
        : errorstr(err)
    {
    }


    virtual ~ReadyException() = default;


    virtual const char* what() const noexcept
    {
        return errorstr.c_str();
    }


private:
    std::string errorstr;
};

class TunInterfaceException : public DBusException
{
public:
    TunInterfaceException(const std::string classname, const std::string msg)
        : DBusException(classname, msg, __FILE__, __LINE__, __FUNCTION__)
    {}
};


/**
 *  Client proxy implementation interacting with a
 *  SessionObject in the session manager over D-Bus
 */
class OpenVPN3SessionProxy : public DBusRequiresQueueProxy
{
public:
    typedef std::shared_ptr<OpenVPN3SessionProxy> Ptr;

    /**
     * Initilizes the D-Bus client proxy.  This constructor will establish
     * the D-Bus connection by itself.
     *
     * @param bus_type   Defines if the connection is on the system or session
     *                   bus.
     * @param objpath    D-Bus object path to the SessionObjectes
     */
    OpenVPN3SessionProxy(GBusType bus_type, std::string objpath)
        : DBusRequiresQueueProxy(bus_type,
                                 OpenVPN3DBus_name_sessions,
                                 OpenVPN3DBus_interf_sessions,
                                 objpath,
                                 "UserInputQueueGetTypeGroup",
                                 "UserInputQueueFetch",
                                 "UserInputQueueCheck",
                                 "UserInputProvide")
    {
        // Only try to ensure the session manager service is available
        // when accessing the main management object
        if (OpenVPN3DBus_rootp_sessions == objpath)
        {
            (void) GetServiceVersion();
        }
    }

    /**
     * Initilizes the D-Bus client proxy.  This constructor will use an
     * existing D-Bus connection object for all D-Bus calls
     *
     * @param dbusobj    DBus connection object
     * @param objpath    D-Bus object path to the SessionObject
     */
    OpenVPN3SessionProxy(DBus & dbusobj, const std::string objpath)
        : DBusRequiresQueueProxy(dbusobj,
                                 OpenVPN3DBus_name_sessions,
                                 OpenVPN3DBus_interf_sessions,
                                 objpath,
                                 "UserInputQueueGetTypeGroup",
                                 "UserInputQueueFetch",
                                 "UserInputQueueCheck",
                                 "UserInputProvide")
    {
        // Only try to ensure the session manager service is available
        // when accessing the main management object
        if (OpenVPN3DBus_rootp_sessions == objpath)
        {
            (void) GetServiceVersion();
        }
    }

    const std::string GetPath() const
    {
        return GetProxyPath();
    }

    /**
     *  Makes the VPN backend client process start the connecting to the
     *  VPN server
     */
    void Connect()
    {
        simple_call("Connect", "Failed to start a new tunnel");
    }

    /**
     *  Makes the VPN backend client process disconnect and then
     *  instantly reconnect to the VPN server
     */
    void Restart()
    {
        simple_call("Restart", "Failed to restart tunnel");
    }


    /**
     *  Disconnects and shuts down the VPN backend process.  This call
     *  will invalidate the current session object.  This can also be used
     *  to shutdown a backend process before doing a Connect() call.
     */
    void Disconnect()
    {
        simple_call("Disconnect", "Failed to disconnect tunnel");
    }


    /**
     *  Pause an on-going VPN tunnel.  Pausing and Resuming an existing VPN
     *  tunnel is generally much faster than doing a full Disconnect() and
     *  Connect() cycle.
     *
     * @param reason  A string provided to the VPN backend process why the
     *                tunnel was suspended.  Primarily used for logging.
     */
    void Pause(std::string reason)
    {
        GVariant *res = Call("Pause",
                             g_variant_new("(s)", reason.c_str()));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to pause tunnel");
        }
        g_variant_unref(res);
    }


    /**
     *   Resumes a paused VPN tunnel
     */
    void Resume()
    {
        simple_call("Resume", "Failed to resume tunnel");
    }


    /**
     *  Checks if the VPN backend process has all it needs to start connecting
     *  to a VPN server.  If it needs more information from the front-end, a
     *  ReadyException will be thrown with more details.
     */
    void Ready()
    {
        try
        {
            simple_call("Ready", "Connection not ready to connect yet");
        }
        catch (DBusException& excp)
        {
            // Throw D-Bus errors related to "Ready" errors as ReadyExceptions
            std::string e(excp.GetRawError());
            if (e.find("net.openvpn.v3.error.ready") != std::string::npos)
            {
                throw ReadyException(e);
            }
            // Otherwise, just rethrow the DBusException
            throw;
        }
    }


    /**
     * Retrieves the last reported status from the VPN backend
     *
     * @return  Returns a populated struct StatusEvent with the full status.
     */
    StatusEvent GetLastStatus()
    {
        GVariant *status = GetProperty("status");
        StatusEvent ret(status);
        g_variant_unref(status);
        return ret;
    }


    /**
     *  Will the session log properties be accessible to users granted
     *  access to the session?
     *
     * @return  Returns false if users can modify receive_log_events
     *          and the log_verbosity properties.  True means only the
     *          session owner has access to the session log via the sesison
     *          manager.
     */
    bool GetRestrictLogAccess()
    {
        return GetBoolProperty("restrict_log_access");
    }


    /**
     *  Change who can access the session log related properties.
     *
     * @param enable  If false, users can modify receive_log_events
     *                and the log_verbosity properties.  True means only the
     *                session owner has access to the session log via the
     *                sesison manager.
     */
    void SetRestrictLogAccess(bool enable)
    {
        SetProperty("restrict_log_access", enable);
    }


    /**
     *  Will the VPN client backend send log messages via
     *  the session manager?
     *
     * @return  Returns true if session manager will proxy log events from
     *          the VPN client backend
     */
    bool GetReceiveLogEvents()
    {
        return GetBoolProperty("receive_log_events");

    }


    /**
     *  Change the session manager log event proxy
     *
     * @param enable  If true, the session manager will proxy log events
     */
    void SetReceiveLogEvents(bool enable)
    {
        SetProperty("receive_log_events", enable);
    }


    /**
     *  Get the log verbosity of the log messages being proxied
     *
     * @return  Returns an integer between 0 and 6,
     *          where 6 is the most verbose.  With 0 only fatal and critical
     *          errors will be provided
     */
    unsigned int GetLogVerbosity()
    {
        return GetUIntProperty("log_verbosity");
    }


    /**
     *  Sets the log verbosity level of proxied log events
     *
     * @param loglevel  An integer between 0 and 6, where 6 is the most
     *                  verbose and 0 the least.
     */
    void SetLogVerbosity(unsigned int loglevel)
    {
        SetProperty("log_verbosity", loglevel);
    }


    /**
     * Retrieve the last log event which has been saved
     *
     * @return Returns a populated struct LogEvent with the the complete log
     *         event.
     */
    LogEvent GetLastLogEvent()
    {
        GVariant *logev = GetProperty("last_log");
        LogEvent ret(logev);
        g_variant_unref(logev);
        return ret;
    }

    /**
     * Retrieves statistics of a running VPN tunnel.  It is gathered by
     * retrieving the 'statistics' session object property.
     *
     * @return Returns a ConnectionStats (std::vector<ConnectionStatDetails>)
     *         array of all gathered statistics.
     */
    ConnectionStats GetConnectionStats()
    {
        GVariant * statsprops = GetProperty("statistics");
        GVariantIter * stats_ar = nullptr;
        g_variant_get(statsprops, "a{sx}", &stats_ar);

        ConnectionStats ret;
        GVariant * r = nullptr;
        while ((r = g_variant_iter_next_value(stats_ar)))
        {
            gchar *key = nullptr;
            gint64 val;
            g_variant_get(r, "{sx}", &key, &val);
            ret.push_back(ConnectionStatDetails(std::string(key), val));
            g_variant_unref(r);
            g_free(key);
        }
        g_variant_iter_free(stats_ar);
        g_variant_unref(statsprops);

        return ret;
    }


    /**
     *  Manipulate the public-access flag.  When public-access is set to
     *  true, everyone have access to this session regardless of how the
     *  access list is configured.
     *
     *  @param public_access Boolean flag.  If set to true, everyone is
     *                       granted read access to the session.
     */
    void SetPublicAccess(bool public_access)
    {
        SetProperty("public_access", public_access);
    }


    /**
     *  Retrieve the public-access flag for session
     *
     * @return Returns true if public-access is granted
     */
    bool GetPublicAccess()
    {
        return GetBoolProperty("public_access");
    }


    /**
     * Grant a user ID (uid) access to this session
     *
     * @param uid  uid_t value of the user which will be granted access
     */
    void AccessGrant(uid_t uid)
    {
        GVariant *res = Call("AccessGrant", g_variant_new("(u)", uid));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "AccessGrant() call failed");
        }
        g_variant_unref(res);
    }


    /**
     * Revoke the access from a user ID (uid) for this session
     *
     * @param uid  uid_t value of the user which will get access revoked
     */
    void AccessRevoke(uid_t uid)
    {
        GVariant *res = Call("AccessRevoke", g_variant_new("(u)", uid));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "AccessRevoke() call failed");
        }
        g_variant_unref(res);
    }


    /**
     *  Enable/Disable the LogEvent forwarding from the client backend
     *
     * @param enable  bool value to enable or disable the forwarding
     */
    void LogForward(bool enable)
    {
        GVariant *res = Call("LogForward", g_variant_new("(b)", enable), false);
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "LogForward() call failed");
        }
        g_variant_unref(res);
    }


    /**
     *  Retrieve the owner UID of this session object
     *
     * @return Returns uid_t of the session object owner
     */
    uid_t GetOwner()
    {
        return GetUIntProperty("owner");
    }


    /**
     *  Retrieve the complete access control list (acl) for this object.
     *  The acl is essentially just an array of user ids (uid)
     *
     * @return Returns an array if uid_t references for each user granted
     *         access.
     */
    std::vector<uid_t> GetAccessList()
    {
        GVariant *res = GetProperty("acl");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "GetAccessList() call failed");
        }
        GVariantIter *acl = NULL;
        g_variant_get(res, "au", &acl);

        GVariant *uid = NULL;
        std::vector<uid_t> ret;
        while ((uid = g_variant_iter_next_value(acl)))
        {
            ret.push_back(g_variant_get_uint32(uid));
            g_variant_unref(uid);
        }
        g_variant_unref(res);
        g_variant_iter_free(acl);
        return ret;
    }


    /**
     *  Retrieve the network interface name used for this tunnel
     *
     * @return  Returns a std::string with the device name
     */
    std::string GetDeviceName() const
    {
        return GetStringProperty("device_name");
    }


    /**
     *  Retrieve if the session is configured to use a Data Channel Offload (DCO)
     *  interface
     *
     * @return  Returns true if DCO is enabled
     */
    bool GetDCO() const
    {
        return GetBoolProperty("dco");
    }

    /**
     *  Enable or disable the Data Channel Offload (DCO) interface.  This can
     *  only be called _before_ starting a connection.  Otherwise it will throw
     *  an exception.
     *
     * @param dco  Boolean, true to enable DCO
     *
     */
    void SetDCO(bool dco) const
    {
        SetProperty("dco", dco);
    }


private:
    /**
     * Simple wrapper for simple D-Bus method calls not requiring much
     * input.  Will also throw a DBusException in case of errors.
     *
     * @param method  D-Bus method to call
     * @param errstr  Error string to provide to the user in case of failures
     */
    void simple_call(std::string method, std::string errstr)
    {
        if (!CheckObjectExists(10, 300))
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                errstr + " (object does not exist)");
        }
        GVariant *res = Call(method);
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                errstr);
        }
        g_variant_unref(res);
    }

};



class OpenVPN3SessionMgrProxy : public DBusProxy
{
public:
    /**
     * Initilizes the D-Bus client proxy.  This constructor will establish
     * the D-Bus connection by itself.
     *
     * @param bus_type   Defines if the connection is on the system or session
     *                   bus.
     * @param objpath    D-Bus object path to the SessionObjectes
     */
    OpenVPN3SessionMgrProxy(GBusType bus_type)
        : DBusProxy(bus_type,
                    OpenVPN3DBus_name_sessions,
                    OpenVPN3DBus_interf_sessions,
                    OpenVPN3DBus_rootp_sessions)
    {
        (void) GetServiceVersion();
    }


    OpenVPN3SessionMgrProxy(DBus & dbusobj)
        : DBusProxy(dbusobj,
                    OpenVPN3DBus_name_sessions,
                    OpenVPN3DBus_interf_sessions,
                    OpenVPN3DBus_rootp_sessions)
    {
        (void) GetServiceVersion();
    }


    /**
     *  Only valid if the session object path points at the main session
     *  manager object.  This starts a new VPN backend client process, running
     *  with the needed privileges.
     *
     * @param cfgpath  VPN profile configuration D-Bus path to use for the
     *                 backend client
     * @return Returns an OpenVPN3SessionProxy::Ptr to the new session
     *         D-Bus object
     */
    OpenVPN3SessionProxy::Ptr NewTunnel(std::string cfgpath)
    {
        if (!g_variant_is_object_path(cfgpath.c_str()))
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionMgrProxy",
                                "Invalid D-Bus path to configuration profile");
        }

        CheckServiceAvail();
        if (!CheckObjectExists(10, 300))
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionMgrProxy",
                                "Failed to connect to session manager");
        }

        GVariant *res = Call("NewTunnel",
                             g_variant_new("(o)", cfgpath.c_str()));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to start a new tunnel");
        }

        gchar *buf = nullptr;
        g_variant_get(res, "(o)", &buf);
        std::string path(buf);
        g_variant_unref(res);
        g_free(buf);

        OpenVPN3SessionProxy::Ptr session;
        session.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, path));
        sleep(1);  // Allow session to be established (FIXME: Signals?)
        return session;
    }


    /**
     * Retrieves an array of strings with session paths which are available
     * to the calling user
     *
     * @return A std::vector<std::string> of session paths
     */
    std::vector<std::string> FetchAvailableSessionPaths()
    {
        GVariant *res = Call("FetchAvailableSessions");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to retrieve available sessions");
        }
        GVariantIter *sesspaths = NULL;
        g_variant_get(res, "(ao)", &sesspaths);

        GVariant *path = NULL;
        std::vector<std::string> ret;
        while ((path = g_variant_iter_next_value(sesspaths)))
        {
            gsize len;
            ret.push_back(std::string(g_variant_get_string(path, &len)));
            g_variant_unref(path);
        }
        g_variant_unref(res);
        g_variant_iter_free(sesspaths);
        return ret;
    }


    /**
     *  Retrieve an array of OpenVPN3SessionProxy objects for all available
     *  sessions
     *
     * @return  std::vector<OpenVPN3SessionProxy::Ptr> of session objects
     */
    std::vector<OpenVPN3SessionProxy::Ptr> FetchAvailableSessions()
    {
        std::vector<OpenVPN3SessionProxy::Ptr> ret;
        for (const auto& path : FetchAvailableSessionPaths())
        {
            OpenVPN3SessionProxy::Ptr s;
            s.reset(new OpenVPN3SessionProxy(G_BUS_TYPE_SYSTEM, path));
            ret.push_back(s);
        }
        return ret;
    }


    std::vector<std::string> FetchManagedInterfaces()
    {
        GVariant *res = Call("FetchManagedInterfaces");
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to retrieve managed interfaces");
        }

        GVariantIter *device_list = nullptr;
        g_variant_get(res, "(as)", &device_list);

        GVariant *device = nullptr;
        std::vector<std::string> ret;
        while ((device = g_variant_iter_next_value(device_list)))
        {
            ret.push_back(std::string(g_variant_get_string(device, nullptr)));
            g_variant_unref(device);
        }
        g_variant_unref(res);
        g_variant_iter_free(device_list);
        return ret;
    }


    /**
     *  Lookup all sessions which where started with the given configuration
     *  profile name.
     *
     * @param cfgname  std::string containing the configuration name to
     *                 look up
     *
     * @return Returns a std::vector<std::string> with all session object
     *         paths which were started with the given configuration name.
     *         If no match is found, the std::vector will be empty.
     */
    std::vector<std::string> LookupConfigName(std::string cfgname)
    {
        GVariant *res = Call("LookupConfigName",
                             g_variant_new("(s)", cfgname.c_str()));
        if (nullptr == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to lookup configuration names");
        }
        GVariantIter *session_paths = nullptr;
        g_variant_get(res, "(ao)", &session_paths);

        GVariant *path = nullptr;
        std::vector<std::string> ret;
        while ((path = g_variant_iter_next_value(session_paths)))
        {
            ret.push_back(GLibUtils::GetVariantValue<std::string>(path));
            g_variant_unref(path);
        }
        g_variant_unref(res);
        g_variant_iter_free(session_paths);
        return ret;
    }


    /**
     *  Lookup the session path for a specific interface name.
     *
     * @param interface  std::string containing the interface name
     *
     * @return  Returns a std::string containing the D-Bus path to the session
     *          this interface is related to.  If not found, and exception
     *          is thrown.
     */
    std::string LookupInterface(std::string interface)
    {
        try
        {
            GVariant *res = Call("LookupInterface",
                                 g_variant_new("(s)", interface.c_str()));
            if (nullptr == res)
            {
                throw TunInterfaceException("LookupInterface",
                                            "Failed to lookup interface");
            }

            std::string ret(GLibUtils::ExtractValue<std::string>(res, 0));
            g_variant_unref(res);

            if (ret.empty())
            {
                throw TunInterfaceException("LookupInterface",
                                            "No managed interface found");
            }
            return ret;
        }
        catch (const TunInterfaceException&)
        {
            throw;
        }
        catch (const DBusException& rawerr)
        {
            std::string err(rawerr.what());
            size_t p = err.find("GDBus.Error:net.openvpn.v3.error.iface:");
            if (p != std::string::npos)
            {
                throw TunInterfaceException("LookupInterface",
                                            err.substr(err.rfind(":")+2));
            }
            throw;
        }
    }
};
