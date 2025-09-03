//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   systemd-resolved-ipaddr.cpp
 *
 * @brief  Helper class to parse IP addresses used by the systemd-resolved
 *         D-Bus API
 */

#include <iomanip>

#include <openvpn/common/split.hpp>

#include "netcfg/dns/systemd-resolved-ipaddr.hpp"


namespace NetCfg::DNS::resolved {

IPAddress::IPAddress(const std::string &addr, int override_family)
{
    if (override_family < 0)
    {
        // IPv6 addresses can contain both ':' and '.'  while IPv4 must
        // contain '.'
        family = (std::string::npos != addr.find(":")
                      ? AF_INET6
                      : (std::string::npos != addr.find(".") ? AF_INET
                                                             : -1));
    }
    else
    {
        family = override_family;
    }

    // Split up the string into address groups, separated by the
    // separator used by the IP address family
    // TODO: This implementation does not support IPv4 addresses in IPv6
    std::vector<std::string> ip;
    int base = 10;
    switch (family)
    {
    case AF_INET:
        ip = openvpn::Split::by_char<std::vector<std::string>,
                                     openvpn::NullLex,
                                     openvpn::Split::NullLimit>(addr, '.');
        base = 10;
        break;

    case AF_INET6:
        ip = openvpn::Split::by_char<std::vector<std::string>,
                                     openvpn::NullLex,
                                     openvpn::Split::NullLimit>(addr, ':');
        base = 16;
        break;

    default:
        break;
    }

    ipaddr = {};
    for (const auto &e : ip)
    {
        if (e.empty())
        {
            switch (family)
            {
            case AF_INET:
                // IPv4 addresses cannot have any empty elements; it is
                // always a value between 0 and 255.
                throw Exception("Invalid data for IPv4 address");

            case AF_INET6:
                // When an IPv6 "address group" is empty, it's the '::'
                // part of an address.  The systemd-resolved API need that to
                // be fully expanded.  There are 8 address group in an IPv6
                // address, so the '::' need to be expanded so it will be a
                // total of 8 groups.  And since each group contains 2 bytes and
                // the systemd-resolved API takes a single byte per per array
                // element, it needs to be provided twice.
                for (uint8_t gr = (9 - ip.size()); gr > 0; --gr)
                {
                    ipaddr.push_back(std::byte(0));
                    ipaddr.push_back(std::byte(0));
                }
            default:
                break;
            }
            continue;
        }

        // Add the numeric value from the string element to the byte value
        // the systemd-resolved API requires
        uint32_t value;
        try
        {
            value = std::stoi(e, nullptr, base);
        }
        catch (const std::exception &)
        {
            throw Exception("Invalid IP address");
        }

        switch (family)
        {
        case AF_INET:
            // IPv4 is straight forward; only a single byte
            if (value > 255)
            {
                throw Exception("Invalid IPv4 address value");
            }
            ipaddr.push_back(std::byte(value));
            break;

        case AF_INET6:
            // Catch IPv4 addresses inside IPv6 addresses, which is not
            // supported
            if (std::string::npos != e.find("."))
            {
                throw Exception("IPv4 in IPv6 addresses is not supported");
            }

            if (value > 0xffff)
            {
                throw Exception("Invalid IPv6 address value");
            }

            // Each IPv6 group need to be split up further into two bytes
            ipaddr.push_back(std::byte((value >> 8) & 0x00ff));
            ipaddr.push_back(std::byte(value & 0x00ff));
            break;

        default:
            break;
        }
    }
    validate_data();
}


IPAddress::IPAddress(GVariant *addr)
{
    glib2::Utils::checkParams(__func__, addr, "(iay)", 2);

    family = glib2::Value::Extract<int>(addr, 0);
    GVariant *ip_array = g_variant_get_child_value(addr, 1);
    ipaddr = glib2::Value::ExtractVector<std::byte>(ip_array, nullptr, false);

    validate_data();
}


std::string IPAddress::str() const
{
    auto bytearray2int = [](const std::vector<std::byte> &b, uint8_t i)
    {
        return std::to_integer<int>(b[i]);
    };

    std::stringstream address;
    switch (family)
    {
    case AF_INET:
        address << std::dec
                << bytearray2int(ipaddr, 0) << "."
                << bytearray2int(ipaddr, 1) << "."
                << bytearray2int(ipaddr, 2) << "."
                << bytearray2int(ipaddr, 3);
        break;

    case AF_INET6:
        // TODO: Implement collapsing of the longest segment of '0000' to '::'
        address << std::hex << std::setfill('0')
                << std::setw(2) << bytearray2int(ipaddr, 0)
                << std::setw(2) << bytearray2int(ipaddr, 1) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 2)
                << std::setw(2) << bytearray2int(ipaddr, 3) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 4)
                << std::setw(2) << bytearray2int(ipaddr, 5) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 6)
                << std::setw(2) << bytearray2int(ipaddr, 7) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 8)
                << std::setw(2) << bytearray2int(ipaddr, 9) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 10)
                << std::setw(2) << bytearray2int(ipaddr, 11) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 12)
                << std::setw(2) << bytearray2int(ipaddr, 13) << ":"
                << std::setw(2) << bytearray2int(ipaddr, 14)
                << std::setw(2) << bytearray2int(ipaddr, 15);
        break;
    }

    return address.str();
}


GVariant *IPAddress::GetGVariant() const
{
    GVariantBuilder *b = glib2::Builder::Create("(iay)");
    glib2::Builder::Add(b, family, "i");
    glib2::Builder::Add(b, glib2::Value::CreateVector(ipaddr));
    return glib2::Builder::Finish(b);
}


void IPAddress::validate_data()
{
    switch (family)
    {
    case AF_INET:
        if (ipaddr.size() != 4)
        {
            throw Exception("Invalid IPv4 address");
        }
        break;

    case AF_INET6:
        if (ipaddr.size() != 16)
        {
            throw Exception("Invalid IPv6 address");
        }
        break;

    default:
        throw Exception("Unsupported IP address family");
    }
}

} // namespace NetCfg::DNS::resolved
