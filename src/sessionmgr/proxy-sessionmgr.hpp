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

#ifndef OPENVPN3_DBUS_PROXY_SESSION_HPP
#define OPENVPN3_DBUS_PROXY_SESSION_HPP

#include <iostream>

#include "dbus/core.hpp"
#include "dbus/requiresqueue-proxy.hpp"

using namespace openvpn;

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

class ReadyException : public DBusException
{
public:
    ReadyException(const std::string& err, const char *filen, const unsigned int linenum, const char *fn) noexcept
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


class OpenVPN3SessionProxy : public DBusRequiresQueueProxy
{
public:
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

    void Connect()
    {
        simple_call("Connect", "Failed to start a new tunnel");
    }

    void Disconnect()
    {
        simple_call("Disconnect", "Failed to disconnect tunnel");
    }

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

    void Resume()
    {
        simple_call("Resume", "Failed to resume tunnel");
    }


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

private:
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
