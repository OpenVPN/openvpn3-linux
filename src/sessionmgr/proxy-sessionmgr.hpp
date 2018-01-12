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
 * @file   proxy-sessionmgr.hpp
 *
 * @brief  Provides a C++ object implementation of a D-Bus session manger
 *         and session object.  This proxy class will perform the D-Bus
 *         calls and provide the results as native C++ data types.
 */


#ifndef OPENVPN3_DBUS_PROXY_SESSION_HPP
#define OPENVPN3_DBUS_PROXY_SESSION_HPP

#include <iostream>

#include "dbus/core.hpp"
#include "dbus/requiresqueue-proxy.hpp"
#include "client/statistics.hpp"

using namespace openvpn;


/**
 * Carries a status record as reported by a VPN backend client
 */
struct BackendStatus
{
    BackendStatus()
    {
        major = StatusMajor::UNSET;
        minor = StatusMinor::UNSET;
        message = "";
    }

    StatusMajor major;
    StatusMinor minor;
    std::string message;
};


/**
 * This exception is thrown when the OpenVPN3SessionProxy::Ready() call
 * indicates the VPN backend client needs more information from the
 * frontend process.
 */
class ReadyException : public DBusException
{
public:
    ReadyException(const std::string& err, const char *filen,
                   const unsigned int linenum, const char *fn) noexcept
        : DBusException("ReadyException", err, filen, linenum, fn)
    {
    }


    virtual ~ReadyException() throw()
    {
    }


    virtual const char* what() const throw()
    {
        std::stringstream ret;
        ret << "[ReadyException: "
            << filename << ":" << line << ", "
            << classname << "::" << method << "()] " << errorstr;
        return ret.str().c_str();
    }


    const std::string& err() const noexcept
    {
        std::string ret(errorstr);
        return std::move(ret);
    }
};
#define THROW_READYEXCEPTION(fault_data) throw ReadyException(fault_data, __FILE__, __LINE__, __FUNCTION__)


/**
 *  Client proxy implementation interacting with a
 *  SessionObject in the session manager over D-Bus
 */
class OpenVPN3SessionProxy : public DBusRequiresQueueProxy
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
    }


    /**
     *  Only valid if the session object path points at the main session
     *  manager object.  This starts a new VPN backend client process, running
     *  with the needed privileges.
     *
     * @param cfgpath  VPN profile configuration D-Bus path to use for the
     *                 backend client
     * @return Returns a D-Bus object path string to the session object
     *         created
     */
    std::string NewTunnel(std::string cfgpath)
    {
        GVariant *res = Call("NewTunnel",
                             g_variant_new("(o)", cfgpath.c_str()));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                "Failed to start a new tunnel");
        }

        gchar *buf = NULL;
        g_variant_get(res, "(o)", &buf);
        std::string ret(buf);
        g_variant_unref(res);
        g_free(buf);

        return ret;
    }


    /**
     * Retrieves an array of strings with session paths which are available
     * to the calling user
     *
     * @return A std::vector<std::string> of session paths
     */
    std::vector<std::string> FetchAvailableSessions()
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
     *  Makes the VPN backend client process start the connecting to the
     *  VPN server
     */
    void Connect()
    {
        simple_call("Connect", "Failed to start a new tunnel");
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
            THROW_READYEXCEPTION(excp.getRawError());
        }
    }


    /**
     * Retrieves the last reported status from the VPN backend
     *
     * @return  Returns a populated struct BackendStatus with the full status.
     */
    BackendStatus GetLastStatus()
    {
        GVariant *status = GetProperty("status");
        BackendStatus ret;
        GVariant *d = nullptr;

        d = g_variant_lookup_value(status, "major", G_VARIANT_TYPE_UINT32);
        ret.major = (StatusMajor) g_variant_get_uint32(d);
        g_variant_unref(d);

        d = g_variant_lookup_value(status, "minor", G_VARIANT_TYPE_UINT32);
        ret.minor = (StatusMinor) g_variant_get_uint32(d);
        g_variant_unref(d);

        gsize len;
        d = g_variant_lookup_value(status,
                                   "status_message", G_VARIANT_TYPE_STRING);
        ret.message = std::string(g_variant_get_string(d, &len));
        g_variant_unref(d);
        if (len != ret.message.size())
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy", "Failed retrieving status message text (inconsisten length)");
        }

        g_variant_unref(status);
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
        }
        return ret;
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
        GVariant *res = Call(method);
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("OpenVPN3SessionProxy",
                                errstr);
        }
    }

};

#endif // OPENVPN3_DBUS_PROXY_CONFIG_HPP
