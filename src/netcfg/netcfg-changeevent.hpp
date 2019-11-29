//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
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

#include <vector>
#include <sstream>
#include <glib.h>

#include "netcfg-exception.hpp"
#include "netcfg-changetype.hpp"


struct NetCfgChangeEvent {
    NetCfgChangeEvent(const NetCfgChangeType& t, const std::string& dev,
                     const NetCfgChangeDetails& d) noexcept;
    NetCfgChangeEvent(GVariant *params);
    NetCfgChangeEvent() noexcept;

    void reset() noexcept;
    bool empty() const noexcept;

    static const std::string IntrospectionXML() noexcept
    {
        return "            <signal name='NetWorkChange'>"
               "                <arg type='u' name='type'/>"
               "                <arg type='s' name='device'/>"
               "                <arg type='a{ss}' name='details'/>"
               "            </signal>";
    }

    static const std::string TypeStr(const NetCfgChangeType& type,
                                     bool tech_form = false) noexcept
    {
        switch (type)
        {
        case NetCfgChangeType::UNSET:
            return "[UNSET]";
        case NetCfgChangeType::DEVICE_ADDED:
            return (tech_form ? "DEVICE_ADDED" : "Device Added");
        case NetCfgChangeType::DEVICE_REMOVED:
            return (tech_form ? "DEVICE_REMOVED" : "Device Removed");
        case NetCfgChangeType::IPADDR_ADDED:
            return (tech_form ? "IPADDR_ADDED" : "IP Address Added");
        case NetCfgChangeType::IPADDR_REMOVED:
            return (tech_form ? "IPADDR_REMOVED" : "IP Address Removed");
        case NetCfgChangeType::ROUTE_ADDED:
            return (tech_form ? "ROUTE_ADDED" : "Route Added");
        case NetCfgChangeType::ROUTE_REMOVED:
            return (tech_form ? "ROUTE_REMOVED" : "Route Removed");
        case NetCfgChangeType::ROUTE_EXCLUDED:
            return (tech_form ? "ROUTE_EXCLUDED" : "Route Excluded");
        case NetCfgChangeType::DNS_SERVER_ADDED:
            return (tech_form ? "DNS_SERVER_ADDED" : "DNS Server Added");
        case NetCfgChangeType::DNS_SERVER_REMOVED:
            return (tech_form ? "DNS_SERVER_REMOVED" : "DNS Server Removed");
        case NetCfgChangeType::DNS_SEARCH_ADDED:
            return (tech_form ? "DNS_SEARCH_ADDED" : "DNS Search domain Added");
        case NetCfgChangeType::DNS_SEARCH_REMOVED:
            return (tech_form ? "DNS_SEARCH_REMOVED" : "DNS Search domain Removed");
        default:
            return "[UNKNOWN: " + std::to_string((uint8_t) type) + "]";
        }
    }


    static const std::vector<std::string> FilterMaskList(const uint16_t mask,
                                           bool tech_form = false)
    {
        std::vector<std::string> ret;

        for (uint16_t i = 0; i < 16; ++i)
        {
            uint16_t flag = 1 << i;
            NetCfgChangeType t = (NetCfgChangeType)(flag);
            if (flag & mask)
            {
                ret.push_back(TypeStr(t, tech_form));
            }
        }

        return ret;
    }

    static const std::string FilterMaskStr(const uint16_t mask,
                                           bool tech_form = false,
                                           const std::string& separator=", ")
    {
        std::stringstream buf;
        bool first_done = false;
        for (const auto& t : FilterMaskList(mask, tech_form))
        {
            buf << (first_done ? separator : "") << t;
            first_done = true;
        }
        std::string ret(buf.str());
        return ret;
    }

    GVariant * GetGVariant() const;

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
    friend std::ostream& operator<<(std::ostream& os, const NetCfgChangeEvent& s)
    {
        if (s.empty())
        {
            return os << "(Empty Network Change Event)";
        }

        std::stringstream detstr;
        bool beginning = true;
        for (const auto& kv : s.details)
        {
            detstr << (beginning ? ": " : ", ")
                   << kv.first << "='" << kv.second << "'";
            beginning = false;
        }
        return os << "Device " << s.device
                  << " - " << TypeStr(s.type)
                  << detstr.str();
    }

    bool operator==(const NetCfgChangeEvent& compare) const;
    bool operator!=(const NetCfgChangeEvent& compare) const;

    NetCfgChangeType type;
    std::string device;
    NetCfgChangeDetails details;
};
