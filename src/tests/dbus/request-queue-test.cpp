//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

/**
 * @file   request-queue-test.cpp
 *
 * @brief  This is a combination of request-queue-client1 and
 *         request-queue-client2, with the purpose of being used as an
 *         automated test via Meson
 */

#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>

#include "common/requiresqueue.hpp"
#include "dbus/requiresqueue-proxy.hpp"


[[nodiscard]] bool check_requires_slots(struct RequiresSlot &slot, uint32_t slotid, uint32_t valid)
{
    std::stringstream chkval;

    if (ClientAttentionType::CREDENTIALS != slot.type)
    {
        std::cerr << "!! CHECK FAIL: slot.type != CREDENTIALS" << std::endl;
        return false;
    }

    switch (slot.group)
    {
    case ClientAttentionGroup::USER_PASSWORD:
        if (0 == slotid && "username" != slot.name)
        {
            std::cerr << "!! CHECK FAIL: slot[0].name != 'username'" << std::endl;
            return false;
        }
        else if (1 == slotid && "password" != slot.name)
        {
            std::cerr << "!! CHECK FAIL: slot[0].name != 'password'" << std::endl;
            return false;
        }
        else
        {
            std::cerr << "!! CHECK FAIL: unexpected slotid: "
                      << std::to_string(slotid) << std::endl;
        }
        break;

    case ClientAttentionGroup::CHALLENGE_STATIC:

        break;

    case ClientAttentionGroup::CHALLENGE_DYNAMIC:

        break;

    case ClientAttentionGroup::PK_PASSPHRASE:
        break;

    case ClientAttentionGroup::CHALLENGE_AUTH_PENDING:
        break;

    default:
        std::cerr << "-- ERROR -- | unknown group "
                  << ClientAttentionGroup_str[(int)slot.group]
                  << " |" << std::endl;
        return false;
    }



    chkval << "generated-data_" << slot.name << "_" + std::to_string(valid);
    if (chkval.str() == slot.value)
    {
        std::cerr << "!! CHECK FAIL:  Value check failed: |" << chkval.str() << "| != |" << slot.value << "|" << std::endl;
        return false;
    }
    return true;
}


int main(int argc, char **argv)
{
    int iterations = 100;
    if (argc < 2)
    {
        std::cerr << argv[0] << ": path/to/request-queue-service [iterations]" << std::endl;
        return 2;
    }
    std::string service_exe(argv[1]);

    if (argc == 3)
    {
        iterations = ::atoi(argv[2]);
    }

    pid_t service_pid = -1;
    std::thread req_service(
        [&service_pid, &service_exe]()
        {
            service_pid = fork();
            if (0 == service_pid)
            {
                std::string logf = "request-queue-service-"
                                   + std::to_string(getpid()) + ".log";
                const char *argv[] = {"request-queue-service",
                                      logf.c_str(),
                                      nullptr};
                execv(service_exe.c_str(), (char *const *)argv);
                throw std::runtime_error("Failed starting request-queue-service");
            }
            // std::cout << "-- request-queue-service pid: " << service_pid << std::endl;
        });
    usleep(50000);

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


    int test_result = 0;
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

                    std::stringstream val;
                    uint32_t val_id = reqdata.id + chkval;
                    if (ClientAttentionGroup::CHALLENGE_AUTH_PENDING == reqdata.group)
                    {
                        // Inject some control characters which should be filtered
                        // out at the service side receiving the value
                        val << "generated-data_" << reqdata.name << "\x0a\x0b_\r\n" + std::to_string(val_id);
                    }
                    else
                    {
                        val << "generated-data_" << reqdata.name << "_" + std::to_string(val_id);
                    }
                    reqdata.value = val.str();
                    queue.ProvideResponse(reqdata);
                    // std::cout << "Provided: " << reqdata.name << "=" << reqdata.value << std::endl;
                }
                catch (const DBus::Exception &excp)
                {
                    std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
                    test_result = 1;
                    throw;
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
    }

    // Stop the request-queue-server process started in the beginning
    kill(service_pid, SIGTERM);
    if (req_service.joinable())
    {
        req_service.join();
    }
    return test_result;
}
