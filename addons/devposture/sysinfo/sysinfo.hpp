//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#pragma once

#include <string>

namespace DevPosture {

struct SysInfo
{
    SysInfo();

    struct Uname
    {
        std::string sysname;
        std::string machine;
        std::string version;
        std::string release;
    };

    struct OSRelease
    {
        std::string version_id;
        std::string id;
        std::string cpe;
        std::string extra_version;
    };

    operator std::string() const
    {
        return "{\n { " + uname.sysname + ", " + uname.machine + ", " + uname.version
               + ", " + uname.release + " },\n { " + os_release.version_id + ", "
               + os_release.id + ", " + os_release.cpe + ", " + os_release.extra_version
               + " }\n}";
    }

    Uname uname;
    OSRelease os_release;
};

struct DateTime
{
    DateTime();

    operator std::string() const
    {
        return "{ " + date + ", " + time + ", " + timezone + " }";
    }

    std::string date;
    std::string time;
    std::string timezone;
};

} // namespace DevPosture
