//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logtag.cpp
 *
 * @brief  Implementation of the LogTag class
 */

#include <string>
#include "logtag.hpp"


//
//  LogTag class implementation
//

LogTag::LogTag(std::string sender, std::string interface, const bool default_encaps)
    : encaps(default_encaps)
{
    tag = std::string("[") + sender + "/" + interface + "]";

    // Create a hash of the tag, used as an index
    std::hash<std::string> hashfunc;
    hash = hashfunc(tag);
}


LogTag::LogTag()
    : tag(), hash(0), encaps(true)
{
}


LogTag::LogTag(const LogTag &cp)
{
    tag = cp.tag;
    hash = cp.hash;
    encaps = cp.encaps;
}


LogTag::~LogTag()
{
    tag = "";
    hash = 0;
    encaps = true;
}


const std::string LogTag::str() const
{
    return LogTag::str(encaps);
}


const std::string LogTag::str(const bool override) const
{
    if (override)
    {
        return std::string("{tag:") + std::string(std::to_string(hash))
               + std::string("}");
    }
    else
    {
        return std::string(std::to_string(hash));
    }
}
