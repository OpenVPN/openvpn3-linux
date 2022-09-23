//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   src/common/platforminfo.cpp
 *
 * @brief  Provides an API for retrieving OS/platform details
 */

#include "platforminfo.hpp"

//
//  PlatformInfoException implementation
//

PlatformInfoException::PlatformInfoException(const std::string& err)
    : errormsg(err)
{
}

const char* PlatformInfoException::what() const noexcept
{
    return errormsg.c_str();
}




//
//  PlatformInfo implementation
//

PlatformInfo::PlatformInfo(GDBusConnection* con)
    : DBusProxy(con,
                "org.freedesktop.hostname1",
                "org.freedesktop.hostname1",
                "/org/freedesktop/hostname1",
                true)
{
    if (con)
    {
        property_proxy = SetupProxy(
            bus_name, "org.freedesktop.DBus.Properties", GetProxyPath());
    }
}


const std::string PlatformInfo::str() const
{
    std::string os_name{};

    try
    {
        os_name = GetStringProperty("OperatingSystemCPEName");
    }
    catch (const DBusException&)
    {
        // Ignore errors; os_name will be empty
    }

    if (os_name.empty())
    {
        try
        {
            os_name = GetStringProperty("OperatingSystemPrettyName");
        }
        catch (const DBusException&)
        {
            // Ignore errors, os_name should be empty
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
