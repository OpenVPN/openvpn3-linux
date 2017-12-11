//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

/**
 * @file   statistics.hpp
 *
 * @brief  Declaration of the connection statistics container
 */

#ifndef OPENVPN3_DBUS_CLIENT_STATISTICS
#define OPENVPN3_DBUS_CLIENT_STATISTICS
/**
 *  Used to deliver connection statistics for the tunnel to the
 *  user front end.  The full result will be provided as an
 *  array (std::vector) ConnectionStatDetails elements.
 */
struct ConnectionStatDetails
{
    ConnectionStatDetails()
        : key(""), value (-1)
    {
    }

    ConnectionStatDetails(const std::string key, const long long value)
        : key(key), value (value)
    {
    }

    const std::string key;
    const long long value;
};

/**
 *  This data type will contain a full set of connection statistics
 */
typedef std::vector<ConnectionStatDetails> ConnectionStats;

#endif // OPENVPN3_DBUS_CLIENT_STATISTICS
