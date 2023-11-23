//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//

/**
 * @file   request-queue-client.cpp
 *
 * @brief  Simple client side unit test of the RequestQueue class.  This
 *         will attempt to connect to the D-Bus service provided by
 *         request-queue-server on the session bus.  It will query for
 *         all unresolved requests prepared by the server side and generate
 *         test data it will provide back to the service.  To check
 *         if this was done correctly, check the server side console as
 *         the generated data will be present there only (by design).
 *         This test will stress the server, by sending 250 requests
 *         rapidly, where it will tell the server to do a reset between
 *         each iteration.
 */

#include <iostream>
#include <sstream>

#include "common/requiresqueue.hpp"
#include "dbus/requiresqueue-proxy.hpp"


void dump_requires_slot(struct RequiresSlot &reqdata, unsigned int id)
{
    std::cout << "          Id: (" << std::to_string(id) << ") " << reqdata.id << std::endl
              << "        Name: " << reqdata.name << std::endl
              << "       Value: " << reqdata.value << std::endl
              << " Description: " << reqdata.user_description << std::endl
              << "Hidden input: " << (reqdata.hidden_input ? "True" : "False") << std::endl
              << "    Provided: " << (reqdata.provided ? "True" : "False") << std::endl
              << "------------------------------------------------------------" << std::endl;
}


int main(int argc, char **argv)
{
    std::cout << "** Using DBusRequiresQueueProxy implementation"
              << std::endl
              << std::endl;

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

    int iterations = 250;
    if (argc == 2)
    {
        iterations = ::atoi(argv[1]);
    }


    for (int i = 0; i < iterations; i++)
    {
        r = proxy->Call(prxtgt, "Init");
        g_variant_unref(r);

        auto res = queue.QueueCheckTypeGroup();
        if (res.empty())
        {
            std::cerr << "-- ERROR -- | empty response from queue.QueueCheckTypeGroup() |" << std::endl;
            return 1;
        }
        bool up, cs, cd, pk, ap;
        up = cs = cd = pk = ap = false;
        uint32_t chkval = rand() + 1;
        for (auto &type_group : res)
        {
            ClientAttentionType type;
            ClientAttentionGroup group;
            std::tie(type, group) = type_group;
            switch (group)
            {
            case ClientAttentionGroup::USER_PASSWORD:
                up = true;
                break;

            case ClientAttentionGroup::CHALLENGE_STATIC:
                cs = true;
                break;

            case ClientAttentionGroup::CHALLENGE_DYNAMIC:
                cd = true;
                break;

            case ClientAttentionGroup::PK_PASSPHRASE:
                pk = true;
                break;

            case ClientAttentionGroup::CHALLENGE_AUTH_PENDING:
                ap = true;
                break;

            default:
                std::cerr << "-- ERROR -- | unknown group " << ClientAttentionGroup_str[(int)group] << " |" << std::endl;
                return 1;
            }

            for (auto &id : queue.QueueCheck(type, group))
            {
                try
                {
                    struct RequiresSlot reqdata;
                    reqdata = queue.QueueFetch(type, group, id);

                    dump_requires_slot(reqdata, id);

                    std::stringstream val;
                    uint32_t val_id = reqdata.id + chkval;
                    val << "generated-data_" << reqdata.name << "_" + std::to_string(val_id);
                    reqdata.value = val.str();
                    queue.ProvideResponse(reqdata);
                    std::cout << "Provided: " << reqdata.name << "=" << reqdata.value << std::endl;
                }
                catch (const DBus::Exception &excp)
                {
                    std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
                    return 1;
                }
            }
        }

        if (!up || !cs || !cd || !pk || !ap)
        {
            std::cerr << "-- ERROR -- | one of type groups is missing |" << std::endl;
            return 1;
        }

        r = proxy->Call(prxtgt,
                        "Validate",
                        g_variant_new("(su)",
                                      "generated-data_",
                                      chkval));
        auto chk = glib2::Value::Extract<bool>(r, 0);
        g_variant_unref(r);
        if (!chk)
        {
            std::cerr << "-- ERROR -- | Validation failed" << std::endl;
            return 1;
        }
        r = proxy->Call(prxtgt, "ServerDumpResponse");
        g_variant_unref(r);
    }
    std::cout << "Done" << std::endl;
    return 0;
}
