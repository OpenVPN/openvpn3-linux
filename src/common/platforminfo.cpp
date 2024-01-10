//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   src/common/platforminfo.cpp
 *
 * @brief  Provides an API for retrieving OS/platform details
 */

#include <gdbuspp/proxy/utils.hpp>

#include "platforminfo.hpp"


//
//  PlatformInfoException implementation
//

PlatformInfoException::PlatformInfoException(const std::string &err)
    : errormsg(err)
{
}


const char *PlatformInfoException::what() const noexcept
{
    return errormsg.c_str();
}



//
//  PlatformInfo implementation
//

PlatformInfo::PlatformInfo(DBus::Connection::Ptr con)
{
    if (!con)
    {
        return;
    }
    proxy = DBus::Proxy::Client::Create(con, "org.freedesktop.hostname1");
    hostname1_tgt = DBus::Proxy::TargetPreset::Create("/org/freedesktop/hostname1",
                                                      "org.freedesktop.hostname1");
}


const bool PlatformInfo::DBusAvailable() const
{
    auto qry = DBus::Proxy::Utils::Query::Create(proxy);
    return qry->Ping();
}


const std::string PlatformInfo::str() const
{
    std::string os_name{};

    if (proxy)
    {
        try
        {
            os_name = proxy->GetProperty<std::string>(hostname1_tgt,
                                                      "OperatingSystemCPEName");
        }
        catch (const DBus::Exception &)
        {
            // Ignore errors; os_name will be empty
        }

        if (os_name.empty())
        {
            try
            {
                os_name = proxy->GetProperty<std::string>(hostname1_tgt,
                                                          "OperatingSystemPrettyName");
            }
            catch (const DBus::Exception &)
            {
                // Ignore errors, os_name will be empty in this case
            }
        }
    }

    // uname() fallback when D-Bus calls fails
    if (os_name.empty())
    {
        struct utsname info = {};
        if (uname(&info) < 0)
        {
            throw PlatformInfoException(
                "Error retrieving platform information");
        }

        os_name = std::string("generic:") + std::string(info.sysname) + "/"
                  + std::string(info.release) + "/" + std::string(info.version)
                  + "/" + std::string(info.machine);
    }
    return os_name;
}
