//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   request-queue-client2.cpp
 *
 * @brief  This is a similar test client as request-queue-client, but it will
 *         only to a single iteration.
 */

#include <iostream>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>

#include "common/requiresqueue.hpp"
#include "dbus/requiresqueue-proxy.hpp"


void dump_requires_slot(ClientAttentionType type, ClientAttentionGroup group, struct RequiresSlot &reqdata)
{
    unsigned int t = (unsigned int)type;
    unsigned int g = (unsigned int)group;

    std::cout << "        Type: [" << std::to_string(t) << "] " << ClientAttentionType_str[t] << std::endl
              << "       Group: [" << std::to_string(g) << "] " << ClientAttentionGroup_str[g] << std::endl
              << "          Id: " << reqdata.id << std::endl
              << "        Name: " << reqdata.name << std::endl
              << "       Value: " << reqdata.value << std::endl
              << " Description: " << reqdata.user_description << std::endl
              << "    Provided: " << (reqdata.provided ? "True" : "False") << std::endl
              << "------------------------------------------------------------" << std::endl;
}



int main()
{
    auto conn = DBus::Connection::Create(DBus::BusType::SESSION);
    DBusRequiresQueueProxy queue(conn,
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "/net/openvpn/v3/tests/features/requiresqueue",
                                 "t_QueueCheckTypeGroup",
                                 "t_QueueFetch",
                                 "t_QueueCheck",
                                 "t_ProvideResponse");
    auto proxy = DBus::Proxy::Client::Create(conn, "net.openvpn.v3.tests.requiresqueue");
    auto prxtgt = DBus::Proxy::TargetPreset::Create("/net/openvpn/v3/tests/features/requiresqueue",
                                                    "net.openvpn.v3.tests.requiresqueue");

    GVariant *r = proxy->Call(prxtgt, "ServerDumpResponse");
    g_variant_unref(r);

    try
    {
        for (auto &type_group : queue.QueueCheckTypeGroup())
        {
            ClientAttentionType type;
            ClientAttentionGroup group;
            std::tie(type, group) = type_group;

            std::vector<struct RequiresSlot> slots;
            queue.QueueFetchAll(slots, type, group);

            for (auto &s : slots)
            {
                dump_requires_slot(type, group, s);
            }
        }
    }
    catch (const DBus::Exception &excp)
    {
        std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
        return 1;
    }
    return 0;
};
