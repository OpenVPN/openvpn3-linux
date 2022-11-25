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
 * @file   src/common/platforminfo.hpp
 *
 * @brief  Provides an API for retrieving OS/platform details
 */

#include <sys/utsname.h>
#include <exception>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"


/**
 *  Exception class for PlatformInfo errors
 *
 */
class PlatformInfoException : public std::exception
{
  public:
    PlatformInfoException(const std::string &err);

    virtual const char *what() const noexcept;

  private:
    std::string errormsg = {};
};



/**
 *  Class which extracts OS/distro related information.
 *
 *  The class will attempt to extract OS information via the
 *  org.freedesktop.hostname1 D-Bus service.  If that fails, it
 *  will extract some generic information from uname()
 *
 */
class PlatformInfo : public DBusProxy
{
  public:
    /**
     *  Construct a new PlatformInfo object
     *
     *  @param con Pointer to a GDBusConnection, for D-Bus calls
     *             to the org.freedesktop.hostname1 service.
     */
    PlatformInfo(GDBusConnection *con);


    /**
     *  Return a string containing OS/distribution details.  It
     *  will first attempt to retrieve the Operating System CPE
     *  value.  If that failes, it will look for the "pretty name"
     *  version.  Failing that, it will extract a few strings from
     *  a uname() call.
     *
     *  @return const std::string
     */
    const std::string str() const;


    /**
     *  ostream << operator for stream printing the PlatformInfo string
     *
     *  @param os              Output stream where the platform info
     *                         will be written
     *  @param pinf            PlatformInfo object to print
     *  @return std::ostream&
     */
    friend std::ostream &operator<<(std::ostream &os, const PlatformInfo &pinf)
    {
        return os << pinf.str();
    }
};