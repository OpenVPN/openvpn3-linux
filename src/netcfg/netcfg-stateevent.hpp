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
 * @file   netcfg-stateevent.hpp
 *
 * @brief  Defines constants and provides structs/object capable of handling
 *         state change events from net.openvpn.v3.netcfg
 */

#pragma once

enum class NetCfgStateType : std::uint8_t {
    UNSET,
    DEVICE_ADDED,
    DEVICE_REMOVED,
    IPv4ADDR_ADDED,
    IPv4ADDR_REMOVED,
    IPv4ROUTE_ADDED,
    IPv4ROUTE_REMOVED,
    IPv6ADDR_ADDED,
    IPv6ADDR_REMOVED,
    IPv6ROUTE_ADDED,
    IPv6ROUTE_REMOVED,
    DNS_SERVER_ADDED,
    DNS_SERVER_REMOVED,
    DNS_SEARCH_ADDED,
    DNS_SEARCH_REMOVED
};


struct NetCfgStateEvent {
    NetCfgStateEvent(const NetCfgStateType& t, const std::string& dev,
                     const std::string& d) noexcept
    {
        reset();
        type = t;
        device = dev;
        details = d;
    }

    NetCfgStateEvent() noexcept
    {
        reset();
    }

    void reset() noexcept
    {
        type = NetCfgStateType::UNSET;
        device.clear();
        details.clear();
    }

    bool empty() const noexcept
    {
        return (NetCfgStateType::UNSET == type
                && device.empty() && details.empty());
    }


    static const std::string IntrospectionXML() noexcept
    {
        return "            <signal name='StateChange'>"
               "                <arg type='u' name='type'/>"
               "                <arg type='s' name='device'/>"
               "                <arg type='s' name='details'/>"
               "            </signal>";
    }

    static const std::string TypeStr(const NetCfgStateType& type ) noexcept
    {
        switch (type)
        {
        case NetCfgStateType::UNSET:
            return "[UNSET]";
        case NetCfgStateType::DEVICE_ADDED:
            return "Device Added";
        case NetCfgStateType::DEVICE_REMOVED:
            return "Device Removed";
        case NetCfgStateType::IPv4ADDR_ADDED:
            return "IPv4 Address Added";
        case NetCfgStateType::IPv4ADDR_REMOVED:
            return "IPv4 Address Removed";
        case NetCfgStateType::IPv4ROUTE_ADDED:
            return "IPv4 Route Added";
        case NetCfgStateType::IPv4ROUTE_REMOVED:
            return "IPv4 Route Removed";
        case NetCfgStateType::IPv6ADDR_ADDED:
            return "IPv6 Address Added";
        case NetCfgStateType::IPv6ADDR_REMOVED:
            return "IPv6 Address Removed";
        case NetCfgStateType::IPv6ROUTE_ADDED:
            return "IPv6 Route Added";
        case NetCfgStateType::IPv6ROUTE_REMOVED:
            return "IPv6 Route Removed";
        case NetCfgStateType::DNS_SERVER_ADDED:
            return "DNS Server Added";
        case NetCfgStateType::DNS_SERVER_REMOVED:
            return "DNS Server Removed";
        case NetCfgStateType::DNS_SEARCH_ADDED:
            return "DNS Search domain Added";
        case NetCfgStateType::DNS_SEARCH_REMOVED:
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
    friend std::ostream& operator<<(std::ostream& os , const NetCfgStateEvent& s)
    {
        if (s.empty())
        {
            return os << "(Empty State Change Event)";
        }
        return os << "Device " << s.device
                  << " - " << TypeStr(s.type)
                  << ": " << s.details;
    }

    NetCfgStateType type;
    std::string device;
    std::string details;
};


