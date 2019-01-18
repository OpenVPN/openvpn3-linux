//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-subscriptions.cpp
 *
 * @brief  Manages NetCfgChangeEvent/NetworkChange signal subscriptions
 */

#include <sstream>

#include <openvpn/common/rc.hpp>

#include "dbus/core.hpp"
#include "netcfg-exception.hpp"
#include "netcfg-signals.hpp"
#include "netcfg-subscriptions.hpp"


void NetCfgSubscriptions::Subscribe(const std::string& sender, uint32_t filter_flags)
{
    if (0 == filter_flags)
    {
        throw NetCfgException("Subscription filter flag must be > 0");
    }
    if (65535 < filter_flags)
    {
        throw NetCfgException("Invalid subscription flag, must be < 65535");
    }
    subscriptions[sender] = (uint16_t) filter_flags;
}


void NetCfgSubscriptions::Unsubscribe(const std::string& subscriber)
{
    if (subscriptions.find(subscriber) == subscriptions.end())
    {
        throw NetCfgException("Subscription not found for '"
                              + subscriber + "'");
    }
    subscriptions.erase(subscriber);
}


GVariant* NetCfgSubscriptions::List()
{
    // Build up the array of subscriber and subscription flags
    GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(su)"));
    if (nullptr == bld)
    {
        throw NetCfgException("System error preparing response "
                              "(NetCfgSubscriptions::List)");
    }

    for (const auto& sub : subscriptions)
    {
        g_variant_builder_add(bld, "(su)",
                              sub.first.c_str(),
                              sub.second);
    }
    return GLibUtils::wrapInTuple(bld);
}
