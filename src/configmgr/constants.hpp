//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 *  @file constants.hpp
 *
 *  @brief  A single place to hold various constants used throughout the
 *          code, with the aim of reducing duplication and accidental
 *          errors.
 */

#pragma once

#include <string>
#include "dbus/constants.hpp"

namespace ConfigManager {

/**
 *  Main atom for composing paths, object and interface names.
 */
constexpr char SERVICE_ID[] = "configuration";

const std::string SERVICE_CONFIGMGR = Constants::GenServiceName(SERVICE_ID);
const std::string INTERFACE_CONFIGMGR = Constants::GenInterface(SERVICE_ID);
const std::string PATH_CONFIGMGR = Constants::GenPath(SERVICE_ID);

const std::string SERVICE_BACKEND = Constants::GenServiceName("backends.be");

} // end of namespace ConfigManager
