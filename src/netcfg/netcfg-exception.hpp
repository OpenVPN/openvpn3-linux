//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2022  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019 - 2022  Lev Stipakov <lev@openvpn.net>
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
    NetCfgException(const std::string &err)
        : errormsg(err)
    {
    }

    ~NetCfgException() = default;

    virtual const char *what() const noexcept
    {
        return errormsg.c_str();
    }

  private:
    std::string errormsg;
};



class NetCfgDeviceException : public std::exception
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

    const char *what() const noexcept override
    {
        return user_error.c_str();
    }

    const char *GetObjectPath() const noexcept
    {
        return object_path.c_str();
    }

#ifdef __GIO_TYPES_H__ // Only add GLib/GDBus methods if this is already used
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
                      const GQuark domain,
                      const guint errcode) const noexcept
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
                      const std::string &domain) const noexcept
    {
        GError *dbuserr = g_dbus_error_new_for_dbus_error(domain.c_str(),
                                                          user_error.c_str());
        g_dbus_method_invocation_return_gerror(invocation, dbuserr);
        g_error_free(dbuserr);
    }
#endif // __GIO_TYPES_H__

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

    virtual const char *what() const noexcept override
    {
        return user_error.c_str();
    }

    const std::string &GetError() const noexcept
    {
        return errormsg;
    }

    const char *GetMethod() const noexcept
    {
        return method.c_str();
    }

  private:
    std::string method;
    std::string errormsg;
    std::string user_error;
};
