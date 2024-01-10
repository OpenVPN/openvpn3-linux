//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-exceptions.hpp
 *
 * @brief  Session Manager specific exception
 */

#pragma once

#include <string>
#include <exception>
#include <gdbuspp/exceptions.hpp>

namespace SessionManager {

class Exception : public DBus::Exception
{
  public:
    Exception(const std::string msg,
              GError *gliberr = nullptr)
        : DBus::Exception("SessionManager", msg, gliberr)
    {
    }
};
} // namespace SessionManager
