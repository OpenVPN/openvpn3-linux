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
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/signals/group.hpp>

#include "netcfg-changeevent.hpp"


class NetCfgSignals;

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
    using NetCfgNotifSubscriptions = std::map<std::string, uint32_t>;

    /**
     *  TODO: Crude subscription owner tracking.  Maps the subscriber callers
     *  unique busname to the UID of the caller
     */
    using NetCfgSubscriptionOwner = std::map<std::string, uid_t>;

    [[nodiscard]] static NetCfgSubscriptions::Ptr Create(std::shared_ptr<NetCfgSignals> sigs,
                                                         DBus::Credentials::Query::Ptr creds)
    {
        return NetCfgSubscriptions::Ptr(new NetCfgSubscriptions(sigs, creds));
    }
    ~NetCfgSubscriptions() = default;


    /**
     *  Adds the needed methods to handle NetworkChange signal subscriptions
     *  to the main DBus::Object::Base object
     *
     * @param object_ptr        Raw DBus::Object::Base pointer where to add
     *                          subscription management
     * @param name_subscribe    std::string, sets the subscribe method name
     * @param name_unsubscribe  std::string, sets the unsubscribe method name
     * @param name_list         std::string, sets the list method name
     */
    void SubscriptionSetup(DBus::Object::Base *object_ptr,
                           const std::string &name_subscribe,
                           const std::string &name_unsubscribe,
                           const std::string &name_list);

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
     *  Removes a subscriber from the subscription list.
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


    /**
     *  TODO: Crude and simplistic way to extract the UID of the
     *  subscription owner
     *
     * @param sender   Unique D-Bus busname of a D-Bus method caller
     * @return uid_t Returns the UID of the owner, if found.  If not found -1
     *         is returned
     */
    uid_t GetSubscriptionOwner(const std::string &sender) const;


  private:
    std::shared_ptr<NetCfgSignals> signals = nullptr;
    DBus::Credentials::Query::Ptr creds_query = nullptr;
    NetCfgNotifSubscriptions subscriptions{};
    NetCfgSubscriptionOwner subscr_owners{};

    NetCfgSubscriptions(std::shared_ptr<NetCfgSignals> signals_,
                        DBus::Credentials::Query::Ptr creds_qry_);

    void method_name_subscribe(DBus::Object::Method::Arguments::Ptr args);
    void method_name_unsubscribe(DBus::Object::Method::Arguments::Ptr args);
    void method_name_list(DBus::Object::Method::Arguments::Ptr args);
};
