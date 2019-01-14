//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-changeevent.hpp
 *
 * @brief  Defines constants and provides structs/object capable of handling
 *         network change events from net.openvpn.v3.netcfg
 */

#pragma once

#include "netcfg-exception.hpp"

enum class NetCfgChangeType : std::uint8_t {
    UNSET,
    DEVICE_ADDED,
    DEVICE_REMOVED,
    IPv4ADDR_ADDED,
    IPv4ADDR_REMOVED,
    IPv4ROUTE_ADDED,
    IPv4ROUTE_REMOVED,
    IPv4ROUTE_EXCLUDED,
    IPv6ADDR_ADDED,
    IPv6ADDR_REMOVED,
    IPv6ROUTE_ADDED,
    IPv6ROUTE_REMOVED,
    IPv6ROUTE_EXCLUDED,
    DNS_SERVER_ADDED,
    DNS_SERVER_REMOVED,
    DNS_SEARCH_ADDED,
    DNS_SEARCH_REMOVED
};


struct NetCfgChangeEvent {
    NetCfgChangeEvent(const NetCfgChangeType& t, const std::string& dev,
                     const std::string& d) noexcept
    {
        reset();
        type = t;
        device = dev;
        details = d;
    }

    NetCfgChangeEvent(GVariant *params)
    {
        std::string g_type(g_variant_get_type_string(params));
        if ("(uss)" != g_type)
        {
            throw NetCfgException(std::string("Invalid GVariant data type: ")
                                  + g_type);
        }

        gchar *dev = nullptr;
        gchar *det = nullptr;
        g_variant_get(params, "(uss)", &type, &dev, &det);

        device = std::string(dev);
        details = std::string(det);
        g_free(dev);
        g_free(det);
    }

    NetCfgChangeEvent() noexcept
    {
        reset();
    }

    void reset() noexcept
    {
        type = NetCfgChangeType::UNSET;
        device.clear();
        details.clear();
    }

    bool empty() const noexcept
    {
        return (NetCfgChangeType::UNSET == type
                && device.empty() && details.empty());
    }


    static const std::string IntrospectionXML() noexcept
    {
        return "            <signal name='NetWorkChange'>"
               "                <arg type='u' name='type'/>"
               "                <arg type='s' name='device'/>"
               "                <arg type='s' name='details'/>"
               "            </signal>";
    }

    static const std::string TypeStr(const NetCfgChangeType& type ) noexcept
    {
        switch (type)
        {
        case NetCfgChangeType::UNSET:
            return "[UNSET]";
        case NetCfgChangeType::DEVICE_ADDED:
            return "Device Added";
        case NetCfgChangeType::DEVICE_REMOVED:
            return "Device Removed";
        case NetCfgChangeType::IPv4ADDR_ADDED:
            return "IPv4 Address Added";
        case NetCfgChangeType::IPv4ADDR_REMOVED:
            return "IPv4 Address Removed";
        case NetCfgChangeType::IPv4ROUTE_ADDED:
            return "IPv4 Route Added";
        case NetCfgChangeType::IPv4ROUTE_REMOVED:
            return "IPv4 Route Removed";
        case NetCfgChangeType::IPv4ROUTE_EXCLUDED:
            return "IPv4 Route Excluded";
        case NetCfgChangeType::IPv6ADDR_ADDED:
            return "IPv6 Address Added";
        case NetCfgChangeType::IPv6ADDR_REMOVED:
            return "IPv6 Address Removed";
        case NetCfgChangeType::IPv6ROUTE_ADDED:
            return "IPv6 Route Added";
        case NetCfgChangeType::IPv6ROUTE_REMOVED:
            return "IPv6 Route Removed";
        case NetCfgChangeType::IPv6ROUTE_EXCLUDED:
            return "IPv6 Route Excluded";
        case NetCfgChangeType::DNS_SERVER_ADDED:
            return "DNS Server Added";
        case NetCfgChangeType::DNS_SERVER_REMOVED:
            return "DNS Server Removed";
        case NetCfgChangeType::DNS_SEARCH_ADDED:
            return "DNS Search domain Added";
        case NetCfgChangeType::DNS_SEARCH_REMOVED:
            return "DNS Search domain Removed";
        default:
            return "[UNKNOWN: " + std::to_string((uint8_t) type) + "]";
        }
    }

    GVariant * GetGVariant() const
    {
        return g_variant_new("(uss)", (guint32) type, device.c_str(),
                             details.c_str());
    }

    /**
     *  Makes it possible to write NetCfgStateEvent in a readable format
     *  via iostreams, such as 'std::cout << state', where state is a
     *  NetCfgStateEvent object.
     *
     * @param os  std::ostream where to write the data
     * @param ev  NetCfgStateEvent to write to the stream
     *
     * @return  Returns the provided std::ostream together with the
     *          decoded NetCfgStateEvent information
     */
    friend std::ostream& operator<<(std::ostream& os , const NetCfgChangeEvent& s)
    {
        if (s.empty())
        {
            return os << "(Empty Network Change Event)";
        }
        return os << "Device " << s.device
                  << " - " << TypeStr(s.type)
                  << (s.details.empty() ? "" : ": " + s.details);
    }


    bool operator==(const NetCfgChangeEvent& compare) const
    {
        return ((compare.type== (const NetCfgChangeType) type)
                && (0 == compare.device.compare(device))
                && (0 == compare.details.compare(details)));
    }


    bool operator!=(const NetCfgChangeEvent& compare) const
    {
        return !(this->operator==(compare));
    }

    NetCfgChangeType type;
    std::string device;
    std::string details;
};


