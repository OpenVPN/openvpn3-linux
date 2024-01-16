//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-subscriptions.hpp
 *
 * @brief  Manages NetCfgChangeEvent/NetworkChange signal subscriptions
 */

#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "netcfg-changeevent.hpp"

/**
 *  External utilities can subscribe to NetworkChange signals from the
 *  netcfg service.  These subscriptions are handled in this class.
 *
 */
class NetCfgSubscriptions
{
  public:
    using Ptr = std::shared_ptr<NetCfgSubscriptions>;

    /**
     *  Contains a list of D-Bus subscribers' unique D-Bus name, mapped against
     *  a filter bit mask built up of bitwise OR of NetCfgChangeType values
     */
    typedef std::map<std::string, uint16_t> NetCfgNotifSubscriptions;

    [[nodiscard]] static NetCfgSubscriptions::Ptr Create()
    {
        return NetCfgSubscriptions::Ptr(new NetCfgSubscriptions);
    }
    ~NetCfgSubscriptions() = default;


    /**
     *  Add a new subscription, with a given filter mask of which events the
     *  subscriber wants to receive.
     *
     * @param sender        std::string containing the D-Bus unique bus name,
     *                      which is used as the subscription name.
     * @param filter_flags  uint32_t carrying all the filter flags.  This data
     *                      type is much bigger than the NetCfgChangeType
     *                      (uint16_t).  This is to allow a better overflow
     *                      check.
     *
     * @throws Throws NetCfgException with an error message which can be sent
     *         back to the caller.
     */
    void Subscribe(const std::string &sender, uint32_t filter_flags);


    /**
     *  Removes a subsriber from the subscription list.
     *
     * @param subscriber  std::string with the subscriber to remove
     *
     * @throws NetCfgException if the subscription cannot be found.
     */
    void Unsubscribe(const std::string &subscriber);


    /**
     *  Retrieve a list of all subscribers and what they have subscribed to.
     *
     * @return  Returns a GVariant object containing an array of subscribers
     *          and the subscription filter mask.
     *          D-Bus data type: (a(su))
     */
    GVariant *List();


    /**
     *  Get a list of subscribers for a specific NetCfgChangeType
     *
     * @param ev  NetCfgChangeEvent object where to extract the
     *            NetCfgChangeType from
     *
     * @return  Returns a std::vector<std::string> of all subscribers who
     *          have subscribed to this change type
     */
    std::vector<std::string> GetSubscribersList(const NetCfgChangeEvent &ev) const;


  private:
    NetCfgNotifSubscriptions subscriptions;

    NetCfgSubscriptions() = default;
};
