//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
 * @file   request-queue-client2.cpp
 *
 * @brief  This is a similar test client as request-queue-client, but it will
 *         only to a single iteration.
 */

#include "dbus/core.hpp"
#include "common/requiresqueue.hpp"
#include "dbus/requiresqueue-proxy.hpp"

using namespace openvpn;

void dump_requires_slot(ClientAttentionType type, ClientAttentionGroup group, struct RequiresSlot& reqdata)
{
    unsigned int t = (unsigned int) type;
    unsigned int g = (unsigned int) group;

    std::cout << "        Type: [" << std::to_string(t) << "] " << ClientAttentionType_str[t] << std::endl
              << "       Group: [" << std::to_string(g) << "] " << ClientAttentionGroup_str[g] << std::endl
              << "          Id: " << reqdata.id << std::endl
              << "        Name: " << reqdata.name << std::endl
              << "       Value: " << reqdata.value << std::endl
              << " Description: " << reqdata.user_description << std::endl
              << "    Provided: " << (reqdata.provided ? "True": "False") << std::endl
              << "------------------------------------------------------------" << std::endl;
}

int main()
{
    DBusRequiresQueueProxy queue(G_BUS_TYPE_SESSION,
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "/net/openvpn/v3/tests/features/requiresqueue",
                                 "t_QueueCheckTypeGroup",
                                 "t_QueueFetch",
                                 "t_QueueCheck",
                                 "t_ProvideResponse");

    queue.Call("ServerDumpResponse", true);

    try
    {
        for (auto& type_group : queue.QueueCheckTypeGroup())
        {
            ClientAttentionType type;
            ClientAttentionGroup group;
            std::tie(type, group) = type_group;

            std::vector<struct RequiresSlot> slots;
            queue.QueueFetchAll(slots, type, group);

            for (auto& s : slots)
            {
                dump_requires_slot(type, group, s);
            }
        }
    }
    catch (DBusException &excp)
    {
        std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
        return 1;
    }
    return 0;
};
