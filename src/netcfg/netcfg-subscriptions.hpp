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
 * @file   netcfg-subscriptions.hpp
 *
 * @brief  Manages NetCfgChangeEvent/NetworkChange signal subscriptions
 */

#pragma once

#include <openvpn/common/rc.hpp>


/**
 *  External utilities can subscribe to NetworkChange signals from the
 *  netcfg service.  These subscriptions are handled in this class.
 *
 */
class NetCfgSubscriptions : public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<NetCfgSubscriptions> Ptr;

    /**
     *  Contains a list of D-Bus subscribers' unique D-Bus name, mapped against
     *  a filter bit mask built up of bitwise OR of NetCfgChangeType values
     */
    typedef std::map<std::string, std::uint16_t> NetCfgNotifSubscriptions;


    NetCfgSubscriptions() = default;


    ~NetCfgSubscriptions() = default;


    /**
     *  Generate a D-Bus introspection XML fragment to be included in a
     *  bigger service introspection response.
     *
     * @param name_subscribe    std::string, sets the subscribe method name
     * @param name_unsubscribe  std::string, sets the unsubscribe method name
     * @param name_list         std::string, sets the list method name
     *
     * @return Returns a std::string with the introspection XML fragment.
     */
    static std::string GenIntrospection(const std::string& name_subscribe,
                                        const std::string& name_unsubscribe,
                                        const std::string& name_list)
    {
        std::stringstream introsp;
        introsp << "        <method name='" << name_subscribe << "'>"
                << "          <arg type='u' direction='in' name='filter' />"
                << "        </method>"
                << "        <method name='" << name_unsubscribe << "'>"
                << "          <arg type='s' direction='in' name='optional_subscriber'/>"
                << "        </method>"
                << "        <method name='" << name_list << "'>"
                << "          <arg type='a(su)' direction='out' name='subscriptions'/>"
                << "        </method>";
        return std::string(introsp.str());
    }


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
    void Subscribe(const std::string& sender, uint32_t filter_flags);


    /**
     *  Removes a subsriber from the subscription list.
     *
     * @param subscriber  std::string with the subscriber to remove
     *
     * @throws NetCfgException if the subscription cannot be found.
     */
    void Unsubscribe(const std::string& subscriber);


    /**
     *  Retrieve a list of all subscribers and what they have subscribed to.
     *
     * @return  Returns a GVariant object containing an array of subscribers
     *          and the subscription filter mask.
     *          D-Bus data type: (a(su))
     */
    GVariant* List();


    /**
     *  Get a list of subscribers for a specific NetCfgChangeType
     *
     * @param ev  NetCfgChangeEvent object where to extract the
     *            NetCfgChangeType from
     *
     * @return  Returns a std::vector<std::string> of all subscribers who
     *          have subscribed to this change type
     */
    std::vector<std::string> GetSubscribersList(const NetCfgChangeEvent& ev) const
    {
        // Loop through all subscribers and identify who wants this
        // notification.
        std::vector<std::string> targets;
        for (const auto& s : subscriptions)
        {
            if ((uint16_t) ev.type & s.second)
            {
                targets.push_back(s.first);
            }
        }
        return targets;
    }


private:
    NetCfgNotifSubscriptions subscriptions;
};
