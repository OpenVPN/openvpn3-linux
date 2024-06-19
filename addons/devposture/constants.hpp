//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#pragma once

#include <dbus/constants.hpp>
#include <filesystem>

namespace DevPosture {

/**
 *  Main atom for composing paths, object and interface names.
 */
constexpr char SERVICE_ID[] = "devposture";

const std::string SERVICE_DEVPOSTURE = Constants::GenServiceName(SERVICE_ID);
const std::string INTERFACE_DEVPOSTURE = Constants::GenInterface(SERVICE_ID);
const std::string PATH_DEVPOSTURE = Constants::GenPath(SERVICE_ID);

} // end of namespace DevPosture
