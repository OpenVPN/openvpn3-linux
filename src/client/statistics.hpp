//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   statistics.hpp
 *
 * @brief  Declaration of the connection statistics container
 */

#pragma once

/**
 *  Used to deliver connection statistics for the tunnel to the
 *  user front end.  The full result will be provided as an
 *  array (std::vector) ConnectionStatDetails elements.
 */
struct ConnectionStatDetails
{
    ConnectionStatDetails()
        : key(""), value(-1)
    {
    }

    ConnectionStatDetails(const std::string key, const long long value)
        : key(key), value(value)
    {
    }

    const std::string key;
    const long long value;
};

/**
 *  This data type will contain a full set of connection statistics
 */
typedef std::vector<ConnectionStatDetails> ConnectionStats;
