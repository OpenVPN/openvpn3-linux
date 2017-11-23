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


    bool Ready()
    {
        try
        {
            simple_call("Ready", "Connection not ready to connect yet");
            return true;
        }
        catch (DBusException& excp)
        {
            return false;
        }
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
