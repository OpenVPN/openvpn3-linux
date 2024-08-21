//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved-ipaddr.hpp
 *
 * @brief  Helper class to parse IP addresses used by the systemd-resolved
 *         D-Bus API
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <gdbuspp/glib2/utils.hpp>

#include "systemd-resolved-exception.hpp"


namespace NetCfg::DNS::resolved {

/**
 *  Helper class to parse and process IPv4 and IPv6 addresses
 *  in the D-Bus data format systemd-resolve uses
 */
class IPAddress
{
  public:
    using List = std::vector<IPAddress>;

    /**
     *  Parse an IP address from a std::string
     *
     * @param addr              std::string containing the IP address
     * @param override_family   enforce the IP version,
     *                          valid values: AF_INET or AF_INET6
     */
    IPAddress(const std::string &addr, int override_family = -1);

    /**
     *  Parse an IP address from a D-Bus value container returned by
     *  the systemd-resolved service.
     *
     *  Expected D-Bus data format: (iay)
     *    i  - int32_t         - IP address family (AF_INET, AF_INET6)
     *    ay - array of bytes  - IP address splitted into an array of bytes.
     *                           IPv4 addresses must have 4 array elements,
     *                           IPv6 addresses must have 16 array elements.
     *
     * @param addr  Pointer to the GVariant object holding the IP address
     */
    IPAddress(GVariant *addr);

    /**
     *  Retrieve a string representation of the parsed IP address
     *
     * @return std::string
     */
    std::string str() const;

    /**
     *  Retrieve the IP address in as a D-Bus value container
     *
     *  D-Bus data type format: (iay)
     *    i  - int32_t         - IP address family (AF_INET, AF_INET6)
     *    ay - array of bytes  - IP address splitted into an array of bytes.
     *                           IPv4 addresses will have 4 array elements,
     *                           IPv6 addresses will have 16 array elements.
     * @return GVariant*
     */
    GVariant *GetGVariant() const;

  private:
    std::vector<std::byte> ipaddr;
    int family;

    /**
     *  Simple validation of the parsed IP address.
     *
     *  @throws NetCfg::DNS::resolved::Exception if the IP address parsing
     *          did not succeed
     */
    void validate_data();
};

} // namespace NetCfg::DNS::resolved
