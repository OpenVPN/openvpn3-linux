//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include "datetime.hpp"
#include "../sysinfo/sysinfo.hpp"

namespace DevPosture {

Module::Dictionary DateTimeModule::Run(const Module::Dictionary &)
{
    DateTime dt;

    return {{"date", dt.date}, {"time", dt.time}, {"timezone", dt.timezone}};
}

} // namespace DevPosture
