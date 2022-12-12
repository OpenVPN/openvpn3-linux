//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//

#include <algorithm>
#include <string>
#include <uuid/uuid.h>

std::string generate_path_uuid(std::string prefix, char delim)
{
    uuid_t uuid;
    char uuid_str[38];
    uuid_generate_random(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    std::string ret(uuid_str);
    std::replace(ret.begin(), ret.end(), '-', delim);

    return (prefix == "" ? ret : prefix + "/" + ret);
}
