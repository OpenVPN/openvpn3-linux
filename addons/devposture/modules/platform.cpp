//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include "platform.hpp"
#include "../sysinfo/sysinfo.hpp"


namespace DevPosture {

Module::Dictionary PlatformModule::Run(const Module::Dictionary &)
{
    DevPosture::SysInfo info;

    return {{"uname_sysname", info.uname.sysname},
            {"uname_machine", info.uname.machine},
            {"uname_version", info.uname.version},
            {"uname_release", info.uname.release},
            {"os_release_version_id", info.os_release.version_id},
            {"os_release_id", info.os_release.id},
            {"os_release_cpe", info.os_release.cpe},
            {"extra_version", info.os_release.extra_version}};
}

} // namespace DevPosture
