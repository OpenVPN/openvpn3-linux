//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 * @file   configmgr-exceptions.hpp
 *
 * @brief  Configuration Manager specific exception
 */

#pragma once

#include <string>
#include <exception>
#include <gdbuspp/exceptions.hpp>


namespace ConfigManager {

class Exception : public DBus::Exception
{
  public:
    Exception(const std::string &msg,
              GError *gliberr = nullptr)
        : DBus::Exception("ConfigurationManager", msg, gliberr)
    {
    }
};

} // namespace ConfigManager
