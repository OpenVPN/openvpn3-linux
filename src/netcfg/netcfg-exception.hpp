//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019 - 2023  Lev Stipakov <lev@openvpn.net>
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
#include <gdbuspp/object/method.hpp>
#include <gdbuspp/proxy.hpp>

class NetCfgException : public DBus::Object::Method::Exception
{
  public:
    NetCfgException(const std::string &err)
        : DBus::Object::Method::Exception(err)
    {
        error_domain = "net.openvpn.v3.netcfg.error";
    }

    ~NetCfgException() = default;

    virtual const char *what() const noexcept
    {
        return errormsg.c_str();
    }

  private:
    std::string errormsg;
};



class NetCfgDeviceException : public DBus::Object::Method::Exception
{
  public:
    NetCfgDeviceException(const std::string &objpath,
                          const std::string &devname,
                          const std::string &errmsg)
        : DBus::Object::Method::Exception(errmsg),
          object_path(objpath),
          device_name(devname),
          errormsg(errmsg)
    {
        error_domain = "net.openvpn.v3.netcfg.device.error";
        user_error = device_name + ": " + errormsg;
    }

    const char *what() const noexcept override
    {
        return user_error.c_str();
    }

    const char *GetObjectPath() const noexcept
    {
        return object_path.c_str();
    }

  private:
    std::string object_path;
    std::string device_name;
    std::string errormsg;
    std::string user_error;
};


class NetCfgProxyException : public DBus::Proxy::Exception
{
  public:
    NetCfgProxyException(const std::string &meth,
                         const std::string &err) noexcept
        : DBus::Proxy::Exception(err),
          method(meth), errormsg(err)
    {
        error_domain = "net.openvpn.v3.netcfg.proxy";
        user_error = method + "(): " + errormsg;
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
    std::string method = {};
    std::string errormsg = {};
    std::string user_error = {};
};
