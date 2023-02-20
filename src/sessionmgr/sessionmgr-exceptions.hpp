//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-exceptions.hpp
 *
 * @brief  Session Manager specific exception
 */

#pragma once

#include <string>
#include <exception>

#include "dbus/exceptions.hpp"

namespace SessionManager {


#define THROW_SESSIONMGR(m) throw SessionManager::Exception(m, __FILE__, __LINE__, __FUNCTION__)
class Exception : public DBusException
{
  public:
    Exception(const std::string msg,
              const char *file,
              const int line,
              const char *method)
        : DBusException("SessionManager", msg, file, line, method)
    {
    }
};
} // namespace SessionManager
