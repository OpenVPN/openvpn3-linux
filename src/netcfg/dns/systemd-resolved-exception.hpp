//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved-exception.hpp
 *
 * @brief  Shared exception class for systemd-resolved related exceptions
 */

#pragma once

#include <exception>
#include <string>

namespace NetCfg::DNS::resolved {

class Exception : public std::exception
{
  public:
    Exception(const std::string &err)
        : errmsg(err)
    {
    }

    virtual const char *what() const noexcept
    {
        return errmsg.c_str();
    }

  private:
    std::string errmsg;
};

} // namespace NetCfg::DNS::resolved
