//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
