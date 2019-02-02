//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2018   OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-2018   David Sommerseth <davids@openvpn.net>
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
 * @file   request-queue-service.cpp
 *
 * @brief  Simple unit test of the RequestQueue class.  This first runs
 *         functional tests on most of the methods provided in the class.
 *         If that passes, a D-Bus service is activated on the session
 *         bus, which can be tested by request-queue-client and
 *         request-queue-client2.
 */

#define DEBUG_REQUIRESQUEUE // Enables debug functions in requiresqueue.hpp

#include <exception>

#include <glib-unix.h>

#include "config.h"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "common/requiresqueue.hpp"
#include "common/utils.hpp"

using namespace openvpn;

class ReqQueueMain : public DBusObject
{
public:
    ReqQueueMain(GDBusConnection *dbuscon,
                 const std::string busname, const std::string interface, const std::string rootpath)
        : DBusObject(rootpath),
          dbuscon(dbuscon),
          creds(dbuscon),
          queue(std::unique_ptr<RequiresQueue>(new RequiresQueue()))
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << rootpath << "'>"
                          << "  <interface name='" << interface << "'>"
                          << RequiresQueue::IntrospectionMethods("t_QueueCheckTypeGroup",
                                                                 "t_QueueFetch",
                                                                 "t_QueueCheck",
                                                                 "t_ProvideResponse")
                          << "    <method name='ServerDumpResponse'/>"
                          << "    <method name='Init'/>"
                          << "  </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);
    }

    ~ReqQueueMain()
    {
        RemoveObject(dbuscon);
    }


    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string object_path,
                              const std::string interface,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invocation)
    {
        try
        {
            std::cout << "sender=" << sender
                      << ", uid=" << std::to_string(creds.GetUID(sender))
                      << ", pid=" << std::to_string(creds.GetPID(sender))
                      << std::endl;
        }
        catch (DBusException& excp)
        {
            std::cout << "Failed to retrieve sender credentials: "
                       << excp.GetRawError();
        }

        if ("t_QueueCheckTypeGroup" == method_name)
        {
            try
            {
                queue->QueueCheckTypeGroup(invocation);
            }
            catch (RequiresQueueException& excp)
            {
                excp.GenerateDBusError(invocation);
            }
            return;
        }
        else if ("t_QueueFetch" == method_name)
        {
            try
            {
                queue->QueueFetch(invocation, params);
            }
            catch (RequiresQueueException& excp)
            {
                excp.GenerateDBusError(invocation);
            }
            return;
        }
        if ("t_QueueCheck" == method_name)
        {
            queue->QueueCheck(invocation, params);
            return;
        }
        else if ("t_ProvideResponse" == method_name)
        {
            try
            {
                queue->UpdateEntry(invocation, params);
            }
            catch (RequiresQueueException& excp)
            {
                excp.GenerateDBusError(invocation);
            }
            return;
        }
        else if ("ServerDumpResponse" == method_name)
        {
            queue->_DumpStdout();
            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }
        else if ("Init" == method_name)
        {
            queue = std::unique_ptr<RequiresQueue>(new RequiresQueue());
            queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::PK_PASSPHRASE,
                              "pk_passphrase", "Test private key passphrase", true);
            queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD,
                              "username", "Test Auth User name", false);
            queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD,
                              "password", "Test Auth Password", true);
            queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_DYNAMIC,
                              "dynamic_challenge", "Test Dynamic Challenge", true);
            queue->RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_STATIC,
                              "static_challenge", "Test Static Challenge", true);
            g_dbus_method_invocation_return_value(invocation, NULL);
            return;
        }
        GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.error.tests.requestqueue",
                                                      "Invalid method call");
        g_dbus_method_invocation_return_gerror(invocation, err);
        g_error_free(err);
    }


    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        THROW_DBUSEXCEPTION("ReqQueueMain", "get property not implemented");
    }

    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("ReqQueueMain", "set property not implemented");
    }

private:
    GDBusConnection *dbuscon;
    DBusConnectionCreds creds;
    std::unique_ptr<RequiresQueue> queue;
};


class ReqQueueServiceDBus : public DBus
{
public:
    ReqQueueServiceDBus()
        : DBus(G_BUS_TYPE_SESSION,
               "net.openvpn.v3.tests.requiresqueue",
               "/net/openvpn/v3/tests/features/requiresqueue",
               "net.openvpn.v3.tests.requiresqueue"),
          mainobj(nullptr)
    {
    }

    ~ReqQueueServiceDBus()
    {
        delete mainobj;
    }

    void callback_bus_acquired()
    {
        mainobj = new ReqQueueMain(GetConnection(), GetBusName(), GetDefaultInterface(), GetRootPath());
        mainobj->RegisterObject(GetConnection());
    }

    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    }

    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        THROW_DBUSEXCEPTION("ReqQueueServiceDBus",
                            "D-Bus name not registered: " + busname);
    }

private:
    ReqQueueMain * mainobj;
};


void selftest()
{
    try
    {

        RequiresQueue queue;

        queue.RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::PK_PASSPHRASE,
                         "pk_passphrase", "Selftest Private key passphrase", true);
        queue.RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD,
                         "username", "Selftest Auth User name", false);
        queue.RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD,
                         "password", "Selftest Auth Password", true);
        queue.RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_DYNAMIC,
                         "dynamic_challenge", "Selftest Dynamic Challenge", true);
        queue.RequireAdd(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_STATIC,
                         "static_challenge", "Selftest static Challenge", true);


        // Test QueueCheckTypeGroup()
        auto type_group = queue.QueueCheckTypeGroup();
        for (auto& tygr : type_group)
        {
            ClientAttentionType t;
            ClientAttentionGroup g;
            std::tie(t, g) = tygr;
            unsigned int ti = (unsigned int) t;
            unsigned int gi = (unsigned int) g;
            std::cout << "-- Request - type [" << std::to_string(ti) << "] " << ClientAttentionType_str[ti]
                      << ", group [" << std::to_string(gi) << "] "<< ClientAttentionGroup_str[gi]
                      << std::endl;

            // Test QueueCheck once we have the type and group values
            auto reqids = queue.QueueCheck(t, g);
            for (auto& id : reqids)
            {
                std::cout << "   id: " << std::to_string(id) << std::endl;

                // ... provide some test data
                std::string gen_value = "selftest_value" + std::to_string((ti*100)+(gi*10)+id);
                queue.UpdateEntry(t, g, id, gen_value);

                // ... retrieve it and compare
                std::string chk_value = queue.GetResponse(t, g, id);
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
        std::cout << "GetResponse('pk_passphrase') = " << queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::PK_PASSPHRASE, "pk_passphrase") << std::endl;
        std::cout << "GetResponse('username') = " << queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "username") << std::endl;
        std::cout << "GetResponse('password') = " << queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "password") << std::endl;
        std::cout << "GetResponse('dynamic_challenge') = " << queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_DYNAMIC, "dynamic_challenge") << std::endl;
        std::cout << "GetResponse('static_challenge') = " << queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::CHALLENGE_STATIC, "static_challenge") << std::endl;

        // Checking some out-of-bounds variables
        try
        {
            queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, 99);
            throw std::runtime_error("Did not fail when calling GetResponse(CREDENTIALS, USER_PASSEORD, 99)");
        }
        catch (RequiresQueueException& excp)
        {
            std::cout << "Passed GetResponse(CREDENTIALS, USER_PASSWORD, 99) out-of-boundary check" << std::endl;
            std::cout << "     Exception caught: " << excp.what() << std::endl;
        }

        try
        {
            queue.GetResponse(ClientAttentionType::CREDENTIALS, ClientAttentionGroup::USER_PASSWORD, "this_variable_name_does_not_exist");
            throw std::runtime_error("Did not fail when calling GetResponse(CREDENTIALS, USER_PASSEORD, 'this_variable_name_does_not_exist')");
        }
        catch (RequiresQueueException& excp)
        {
            std::cout << "Passed GetResponse(CREDENTIALS, USER_PASSWORD, '...') unknown variable check" << std::endl;
            std::cout << "     Exception caught: " << excp.what() << std::endl;
        }
    }
    catch (std::exception& excp)
    {
        std::cout << "** EXCEPTION: "<< excp.what() << std::endl;
        exit(2);
    }
}


int main()
{
    // Simple local method tests, before we enable the D-Bus tests
    std::cout << "** Internal API tests" << std::endl;
    selftest();

    // D-Bus tests
    std::cout << std::endl << "** Starting D-Bus server" << std::endl;
    ReqQueueServiceDBus reqqueue;
    reqqueue.Setup();

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
    return 0;
}
