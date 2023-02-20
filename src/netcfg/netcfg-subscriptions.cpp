//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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


void NetCfgSubscriptions::Subscribe(const std::string &sender, uint32_t filter_flags)
{
    if (0 == filter_flags)
    {
        throw NetCfgException("Subscription filter flag must be > 0");
    }
    if (65535 < filter_flags)
    {
        throw NetCfgException("Invalid subscription flag, must be < 65535");
    }
    subscriptions[sender] = (uint16_t)filter_flags;
}


void NetCfgSubscriptions::Unsubscribe(const std::string &subscriber)
{
    if (subscriptions.find(subscriber) == subscriptions.end())
    {
        throw NetCfgException("Subscription not found for '"
                              + subscriber + "'");
    }
    subscriptions.erase(subscriber);
}


GVariant *NetCfgSubscriptions::List()
{
    // Build up the array of subscriber and subscription flags
    GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(su)"));
    if (nullptr == bld)
    {
        throw NetCfgException("System error preparing response "
                              "(NetCfgSubscriptions::List)");
    }

    for (const auto &sub : subscriptions)
    {
        g_variant_builder_add(bld, "(su)", sub.first.c_str(), sub.second);
    }
    return GLibUtils::wrapInTuple(bld);
}
