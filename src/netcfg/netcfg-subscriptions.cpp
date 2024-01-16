//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-subscriptions.cpp
 *
 * @brief  Manages NetCfgChangeEvent/NetworkChange signal subscriptions
 */

#include <cstdint>
#include <sstream>
#include <gdbuspp/glib2/utils.hpp>

#include "netcfg-exception.hpp"
#include "netcfg-subscriptions.hpp"
#include "netcfg-signals.hpp"


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
    GVariantBuilder *bld = glib2::Builder::Create("a(su)");
    if (nullptr == bld)
    {
        throw NetCfgException("System error preparing response "
                              "(NetCfgSubscriptions::List)");
    }

    for (const auto &sub : subscriptions)
    {
        g_variant_builder_add(bld, "(su)", sub.first.c_str(), sub.second);
    }
    return glib2::Builder::FinishWrapped(bld);
}


std::vector<std::string> NetCfgSubscriptions::GetSubscribersList(const NetCfgChangeEvent &ev) const
{
    // Loop through all subscribers and identify who wants this
    // notification.
    std::vector<std::string> targets;
    for (const auto &s : subscriptions)
    {
        if ((uint16_t)ev.type & s.second)
        {
            targets.push_back(s.first);
        }
    }
    return targets;
}
