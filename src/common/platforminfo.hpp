//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   src/common/platforminfo.hpp
 *
 * @brief  Provides an API for retrieving OS/platform details
 */

#include <sys/utsname.h>
#include <exception>
#include <gdbuspp/proxy.hpp>

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
class PlatformInfo
{
  public:
    /**
     *  Construct a new PlatformInfo object
     *
     *  @param con DBus::Connection used to access the
     *             org.freedesktop.hostname1 service.
     */
    PlatformInfo(DBus::Connection::Ptr con);


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

  private:
    DBus::Proxy::Client::Ptr proxy{nullptr};
    DBus::Proxy::TargetPreset::Ptr hostname1_tgt{nullptr};
};
