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
 * @file   netcfg-exception.hpp
 *
 * @brief  Exception classes related to error handling in the
 *         net.openvpn.v3.netcfg service
 */

#pragma once

#include <string>
#include <exception>


class NetCfgException : public std::exception
{
public:
    NetCfgException(const std::string& err)
                : errormsg(err)
    {
    }

    ~NetCfgException() = default;

    virtual const char* what() const noexcept
    {
        return errormsg.c_str();
    }


private:
    std::string errormsg;
};


class NetCfgDeviceException : std::exception
{
public:
    NetCfgDeviceException(const std::string objpath,
                          const std::string devname,
                          const std::string errmsg)
        : object_path(objpath),
          device_name(devname),
          errormsg(errmsg)
    {
        user_error = device_name + ": " + errmsg;
    }

    const char* what() const noexcept override
    {
        return user_error.c_str();
    }


    const char* GetObjectPath() const noexcept
    {
        return object_path.c_str();
    }


#ifdef __GIO_TYPES_H__  // Only add GLib/GDBus methods if this is already used
    /**
     *  Wrapper to more easily return a NetCfgDeviceException
     *  back to an on going D-Bus method call.  This will transport the
     *  error back to the D-Bus caller.
     *
     * @param error      Pointer to a GError pointer where the error will
     *                   be saved
     * @param domain     Error Quark domain used to classify the error
     * @param code       A GIO error code, used for further error
     *                   error classification.  Look up G_IO_ERROR_*
     *                   entries in glib-2.0/gio/gioenums.h for details.
     */
    void SetDBusError(GError **error,
                      const GQuark domain, const guint errcode) const noexcept
    {
        g_set_error(error, domain, errcode, "%s", user_error.c_str());
    }

    /**
     *  Wrapper to more easily return a NetCfgDeviceException
     *  back to an on going D-Bus method call.  This will transport the
     *  error back to the D-Bus caller.
     *
     * @param invocation    Pointer to a invocation object of the on-going
     *                      method call
     *  @param domain       String which classifies the error (QuarkDomain)
     */
    void SetDBusError(GDBusMethodInvocation *invocation,
                      const std::string& domain) const noexcept
    {
        GError *dbuserr = g_dbus_error_new_for_dbus_error(domain.c_str(),
                                                          user_error.c_str());
        g_dbus_method_invocation_return_gerror(invocation, dbuserr);
        g_error_free(dbuserr);
    }
#endif  // __GIO_TYPES_H__


private:
    std::string object_path;
    std::string device_name;
    std::string errormsg;
    std::string user_error;
};


class NetCfgProxyException : public std::exception
{
public:
    NetCfgProxyException(std::string method, std::string err) noexcept
        : method(std::move(method)), errormsg(std::move(err)),
          user_error(method + "(): " + err)
    {

    }

    ~NetCfgProxyException() override = default;

    virtual const char* what() const noexcept override
    {
        return user_error.c_str();
    }

    const std::string& GetError() const noexcept
    {
        return errormsg;
    }

    const char* GetMethod() const noexcept
    {
        return method.c_str();
    }

private:
    std::string method;
    std::string errormsg;
    std::string user_error;

};
