//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  John Eismeier <john.eismeier@gmail.com>
//

/**
 * @file   request-queue-service.cpp
 *
 * @brief  Simple unit test of the RequestQueue class.  This first runs
 *         functional tests on most of the methods provided in the class.
 *         If that passes, a D-Bus service is activated on the session
 *         bus, which can be tested by request-queue-client1 and
 *         request-queue-client2.
 */

#include <exception>
#include <glib-unix.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/service.hpp>
#include <iostream>
#include <memory>
#include <mutex>

#include "build-config.h"

#include "common/requiresqueue.hpp"
#include "common/utils.hpp"


/**
 *  An extended RequiresQueue class which adds a few debug features
 *  to inspect the contents of the RequiresQueue data.
 */
class RequiresQueueDebug : public RequiresQueue
{
  public:
    using Ptr = std::shared_ptr<RequiresQueueDebug>;

    [[nodiscard]] static RequiresQueueDebug::Ptr Create()
    {
        return RequiresQueueDebug::Ptr(new RequiresQueueDebug());
    }


    /**
     *  Dumps the current active queue to stdout
     */
    void _DumpStdout()
    {
        _DumpQueue(std::cout);
    }


    /**
     *  Dumps all the RequiresSlot items of a current RequiresQueue to the
     *  provided output stream
     *
     *  @param logdst   Output stream where to put the dump
     */
    void _DumpQueue(std::ostream &logdst)
    {
        for (auto &e : slots)
        {
            logdst << "          Id: " << e.id << std::endl
                   << "         Key: " << e.name << std::endl
                   << "        Type: [" << std::to_string((int)e.type) << "] "
                   << ClientAttentionType_str[(int)e.type] << std::endl
                   << "       Group: [" << std::to_string((int)e.group) << "] "
                   << ClientAttentionGroup_str[(int)e.group] << std::endl
                   << "       Value: " << e.value << std::endl
                   << " Description: " << e.user_description << std::endl
                   << "Hidden input: " << (e.hidden_input ? "True" : "False")
                   << std::endl
                   << "    Provided: " << (e.provided ? "True" : "False")
                   << std::endl
                   << "-----------------------------------------------------"
                   << std::endl;
        }
    }


    /**
     *  Get access to all the RequiresSlot items
     *
     * @return const std::vector<struct RequiresSlot>
     */
    const std::vector<struct RequiresSlot> DumpSlots() const
    {
        return slots;
    }

  private:
    RequiresQueueDebug()
        : RequiresQueue()
    {
    }
};


class ReqQueueMain : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<ReqQueueMain>;

    ReqQueueMain(DBus::Connection::Ptr conn,
                 const std::string &path,
                 const std::string &interface)
        : DBus::Object::Base(path, interface),
          dbuscon(conn)
    {
        queue = RequiresQueueDebug::Create();
        queue->QueueSetup(this,
                          "t_QueueCheckTypeGroup",
                          "t_QueueFetch",
                          "t_QueueCheck",
                          "t_ProvideResponse");
        init();

        AddMethod("ServerDumpResponse",
                  [this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      std::lock_guard<std::mutex> lg(init_mtx);
                      queue->_DumpStdout();
                      args->SetMethodReturn(nullptr);
                  });

        AddMethod("Init",
                  [this](DBus::Object::Method::Arguments::Ptr args)
                  {
                      this->init();
                      args->SetMethodReturn(nullptr);
                  });

        auto validate = AddMethod("Validate",
                                  [this](DBus::Object::Method::Arguments::Ptr args)
                                  {
                                      auto r = this->Validate(args->GetMethodParameters());
                                      args->SetMethodReturn(r);
                                  });
        validate->AddInput("response_prefix", "s");
        validate->AddInput("iteration", "u");
        validate->AddOutput("result", "b");
    }
    ~ReqQueueMain() = default;


    const bool Authorize(const DBus::Authz::Request::Ptr request) override
    {
        std::cout << "Authorize: " << request << std::endl;
        return true;
    }


  private:
    DBus::Connection::Ptr dbuscon{nullptr};
    RequiresQueueDebug::Ptr queue{nullptr};
    std::mutex init_mtx;

    void init()
    {
        std::lock_guard<std::mutex> lg(init_mtx);

        queue->ClearAll();
        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::PK_PASSPHRASE,
                          "pk_passphrase",
                          "Test private key passphrase",
                          true);

        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::USER_PASSWORD,
                          "username",
                          "Test Auth User name",
                          false);

        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::USER_PASSWORD,
                          "password",
                          "Test Auth Password",
                          true);

        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::CHALLENGE_DYNAMIC,
                          "dynamic_challenge",
                          "Test Dynamic Challenge",
                          true);

        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::CHALLENGE_STATIC,
                          "static_challenge",
                          "Test Static Challenge",
                          true);

        queue->RequireAdd(ClientAttentionType::CREDENTIALS,
                          ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                          "auth_pending",
                          "Pending Auth Challenge",
                          false);
    }


    GVariant *Validate(GVariant *param)
    {
        glib2::Utils::checkParams(__func__, param, "(su)", 2);
        std::string resp_prefix{glib2::Value::Extract<std::string>(param, 0)};
        uint32_t iteration{glib2::Value::Extract<uint32_t>(param, 1)};

        bool result = true;
        for (const auto &slot : queue->DumpSlots())
        {
            std::ostringstream match_val;
            match_val << resp_prefix
                      << slot.name << "_"
                      << std::to_string(iteration + slot.id);
            if ((slot.provided && match_val.str() != slot.value))
            {
                result = false;
                std::cout << "FAILED: [" << std::to_string(slot.id) << "] "
                          << "name=" << slot.name << ", "
                          << "value='" << slot.value << "', "
                          << "expected='" << match_val.str() << "'"
                          << std::endl;
            }
            else if (!slot.provided)
            {
                result = false;
                std::cout << "FAILED: [" << std::to_string(slot.id) << "] "
                          << "name=" << slot.name << " -- not provided "
                          << std::endl;
            }
            else
            {
                std::cout << "Passed: ["
                          << std::to_string(slot.id) << "] "
                          << "name=" << slot.name
                          << "value='" << slot.value
                          << std::endl;
            }
        }
        return glib2::Value::CreateTupleWrapped(result);
    }
};



class ReqQueueService : public DBus::Service
{
  public:
    ReqQueueService(DBus::Connection::Ptr conn)
        : DBus::Service(conn, "net.openvpn.v3.tests.requiresqueue")
    {
    }

    void BusNameAcquired(GDBusConnection *conn, const std::string &busname) override
    {
        std::cout << "Service registered: " << busname << std::endl;
    }

    void BusNameLost(GDBusConnection *conn, const std::string &busname) override
    {
        std::cout << "Lost the bus name: " << busname << std::endl;
        DBus::Service::Stop();
    }
};



void selftest()
{
    try
    {

        RequiresQueue::Ptr queue = RequiresQueue::Create();

        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::PK_PASSPHRASE, "pk_passphrase", "Selftest Private key passphrase", true);
        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "username", "Selftest Auth User name", false);
        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "password", "Selftest Auth Password", true);
        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_DYNAMIC, "dynamic_challenge", "Selftest Dynamic Challenge", true);
        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_STATIC, "static_challenge", "Selftest static Challenge", true);
        queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_AUTH_PENDING, "auth_pending", "Selftest Pending Auth", false);


        // Test QueueCheckTypeGroup()
        for (const auto &tygr : queue->QueueCheckTypeGroup())
        {
            ClientAttentionType t;
            ClientAttentionGroup g;
            std::tie(t, g) = tygr;
            unsigned int ti = (unsigned int)t;
            unsigned int gi = (unsigned int)g;
            std::cout << "-- Request - type [" << std::to_string(ti) << "] " << ClientAttentionType_str[ti]
                      << ", group [" << std::to_string(gi) << "] " << ClientAttentionGroup_str[gi]
                      << std::endl;

            // Test QueueCheck once we have the type and group values
            for (const auto &id : queue->QueueCheck(t, g))
            {
                std::cout << "   id: " << std::to_string(id) << std::endl;

                // ... provide some test data
                std::string gen_value = "selftest_value" + std::to_string((ti * 100) + (gi * 10) + id);
                queue->UpdateEntry(t, g, id, gen_value);

                // ... retrieve it and compare
                std::string chk_value = queue->GetResponse(t, g, id);
                if (chk_value != gen_value)
                {
                    std::stringstream err;
                    err << "Failed comparing generated value '" << gen_value
                        << "' with retrieved value '" << chk_value << "'";
                    throw std::runtime_error(err.str());
                }
                else
                {
                    std::cout << "Passed GetResponse(" << std::to_string(ti)
                              << ", " << std::to_string(gi)
                              << ", " << std::to_string(id)
                              << " check.  Value: '" << chk_value << "'"
                              << std::endl;
                }
            }
        }

        // Check retrieving values via variable names
        std::cout << "GetResponse('pk_passphrase') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::PK_PASSPHRASE,
                                        "pk_passphrase")
                  << std::endl;
        std::cout << "GetResponse('username') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::USER_PASSWORD,
                                        "username")
                  << std::endl;
        std::cout << "GetResponse('password') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::USER_PASSWORD,
                                        "password")
                  << std::endl;
        std::cout << "GetResponse('dynamic_challenge') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::CHALLENGE_DYNAMIC,
                                        "dynamic_challenge")
                  << std::endl;
        std::cout << "GetResponse('static_challenge') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::CHALLENGE_STATIC,
                                        "static_challenge")
                  << std::endl;
        std::cout << "GetResponse('auth_pending') = "
                  << queue->GetResponse(ClientAttentionType::CREDENTIALS,
                                        ClientAttentionGroup::CHALLENGE_AUTH_PENDING,
                                        "auth_pending")
                  << std::endl;

        // Checking some out-of-bounds variables
        try
        {
            queue->GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, 99);
            throw std::runtime_error("Did not fail when calling GetResponse(CREDENTIALS, USER_PASSEORD, 99)");
        }
        catch (const RequiresQueueException &excp)
        {
            std::cout << "Passed GetResponse(CREDENTIALS, USER_PASSWORD, 99) out-of-boundary check" << std::endl;
            std::cout << "     Exception caught: " << excp.what() << std::endl;
        }

        try
        {
            queue->GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "this_variable_name_does_not_exist");
            throw std::runtime_error("Did not fail when calling GetResponse(CREDENTIALS, USER_PASSEORD, 'this_variable_name_does_not_exist')");
        }
        catch (const RequiresQueueException &excp)
        {
            std::cout << "Passed GetResponse(CREDENTIALS, USER_PASSWORD, '...') unknown variable check" << std::endl;
            std::cout << "     Exception caught: " << excp.what() << std::endl;
        }
    }
    catch (const std::exception &excp)
    {
        std::cout << "** EXCEPTION: " << excp.what() << std::endl;
        exit(2);
    }
}



int main()
{
    // Simple local method tests, before we enable the D-Bus tests
    std::cout << "** Internal API tests" << std::endl;
    selftest();

    // D-Bus tests
    std::cout << std::endl
              << "** Starting D-Bus server" << std::endl;
    auto conn = DBus::Connection::Create(DBus::BusType::SESSION);
    auto service = DBus::Service::Create<ReqQueueService>(conn);
    service->CreateServiceHandler<ReqQueueMain>(conn,
                                                "/net/openvpn/v3/tests/features/requiresqueue",
                                                "net.openvpn.v3.tests.requiresqueue");
    service->Run();
    return 0;
}
