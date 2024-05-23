//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019-  David Sommerseth <davids@openvpn.net>
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

LogTag::Ptr LogTag::Create(const std::string &sender,
                           const std::string &interface,
                           const bool default_encaps)
{
    return LogTag::Ptr(new LogTag(sender, interface, default_encaps));
}


LogTag::LogTag(std::string sender, std::string interface, const bool default_encaps)
    : encaps(default_encaps)
{
    tag = std::string("[") + sender + "/" + interface + "]";

    // Create a hash of the tag, used as an index
    std::hash<std::string> hashfunc;
    hash = hashfunc(tag);
}


LogTag::LogTag(const LogTag &cp)
{
    tag = cp.tag;
    hash = cp.hash;
    encaps = cp.encaps;
}


const std::string LogTag::str() const
{
    return LogTag::str(encaps);
}


const std::string LogTag::str(const bool encaps_override) const
{
    if (encaps_override)
    {
        return std::string("{tag:") + std::string(std::to_string(hash))
               + std::string("}");
    }
    else
    {
        return std::string(std::to_string(hash));
    }
}
