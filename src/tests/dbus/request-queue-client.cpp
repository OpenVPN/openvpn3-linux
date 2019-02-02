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

#include <sstream>

#include "dbus/core.hpp"
#include "common/requiresqueue.hpp"

using namespace openvpn;

//#define DIRECT_API_CHECK  // Runs D-Bus call directly via DBusProxy instead of DBusRequiresQueueProxy implementation

void dump_requires_slot(struct RequiresSlot& reqdata, unsigned int id)
{
    std::cout << "          Id: (" << std::to_string(id) << ") " << reqdata.id << std::endl
              << "        Name: " << reqdata.name << std::endl
              << "       Value: " << reqdata.value << std::endl
              << " Description: " << reqdata.user_description << std::endl
              << "Hidden input: " << (reqdata.hidden_input ? "True": "False") << std::endl
              << "    Provided: " << (reqdata.provided ? "True": "False") << std::endl
              << "------------------------------------------------------------" << std::endl;
}

#ifndef DIRECT_API_CHECK

#include "dbus/requiresqueue-proxy.hpp"

int main()
{
    std::cout << "** Using DBusRequiresQueueProxy implementation"
              << std::endl << std::endl;

    DBusRequiresQueueProxy queue(G_BUS_TYPE_SESSION,
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "net.openvpn.v3.tests.requiresqueue",
                                 "/net/openvpn/v3/tests/features/requiresqueue",
                                 "t_QueueCheckTypeGroup",
                                 "t_QueueFetch",
                                 "t_QueueCheck",
                                 "t_ProvideResponse");

    queue.Call("ServerDumpResponse", true);

    for (int i = 0; i < 250; i++)
    {
        queue.Call("Init", true);
        auto res = queue.QueueCheckTypeGroup();
        if (res.empty())
        {
            std::cerr << "-- ERROR -- | empty response from queue.QueueCheckTypeGroup() |" << std::endl;
            return 1;
        }
        bool up, cs, cd, pk;
        up = cs = cd = pk = false;
        for (auto& type_group : res)
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

                default:
                    std::cerr << "-- ERROR -- | unknown group " << ClientAttentionGroup_str[(int)group] << " |" << std::endl;
                    return 1;
            }
            for (auto& id : queue.QueueCheck(type, group))
            {
                try
                {
                    struct RequiresSlot reqdata;
                    reqdata = queue.QueueFetch(type, group, id);

                    dump_requires_slot(reqdata, id);

                    std::stringstream val;
                    val << "generated-data_" << reqdata.name << "_" + std::to_string(reqdata.id + i);
                    reqdata.value = val.str();
                    queue.ProvideResponse(reqdata);
                    std::cout << "Provided: " << reqdata.name << "=" << reqdata.value << std::endl;
                }
                catch (DBusException &excp)
                {
                    std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
                    return 1;
                }
            }
        }
        if (!up || !cs || !cd || !pk)
        {
            std::cerr << "-- ERROR -- | one of type groups is missing |" << std::endl;
            return 1;
        }
        queue.Call("ServerDumpResponse");
    }
    std::cout << "Done" << std::endl;
    return 0;
};

#else // DIRECT_API_CHECK

void deserialize(struct RequiresSlot& result, GVariant *indata)
{
    if (!indata)
    {
        throw RequiresQueueException("indata GVariant pointer is NULL");
    }

    std::string data_type = std::string(g_variant_get_type_string(indata));
    if ("(uuussb)" == data_type)
    {
        //
        // Typically used by the function popping elements from the RequiresQueue,
        // usually a user-frontend application
        //
        if (result.id != 0 || result.provided || !result.value.empty())
        {
            throw RequiresQueueException("RequiresSlot destination is not empty/unused");
        }
        guint32 type = 0;
        guint32 group = 0;
        guint32 id = 0;
        gchar *name = NULL;
        gchar *descr = NULL;
        gboolean hidden_input = false;

        g_variant_get(indata, "(uuussb)",
                      &type,
                      &group,
                      &id,
                      &name,
                      &descr,
                      &hidden_input);
        result.type = (ClientAttentionType) type;
        result.group = (ClientAttentionGroup) group;
        result.id = id;
        if (name)
        {
            std::string name_s(name);
            result.name = name_s;
            g_free(name);
        }
        if (descr)
        {
            std::string descr_s(descr);
            result.user_description = descr_s;
            g_free(descr);
        }
    }
    else
    {
        std::cerr << "Unknown data type: " << data_type << std::endl;
        throw RequiresQueueException("Unknown input data formatting ");
    }
}


int main()
{
    std::cout << "** Using D-Bus API directly" << std::endl << std::endl;

    DBusProxy proxy(G_BUS_TYPE_SESSION,
                    "net.openvpn.v3.tests.requiresqueue",
                    "net.openvpn.v3.tests.requiresqueue",
                    "/net/openvpn/v3/tests/features/requiresqueue");

    proxy.Call("ServerDumpResponse", true);
    proxy.Call("Init", true);

    for (int group = (int)ClientAttentionGroup::USER_PASSWORD;
         group <= (int)ClientAttentionGroup::CHALLENGE_DYNAMIC; ++group)
    {
        GVariant *res = proxy.Call("t_QueueCheck",
                                   g_variant_new("(uu)",
                                                 ClientAttentionType::CREDENTIALS,
                                                 (ClientAttentionGroup) group));
        GVariantIter *array = 0;
        g_variant_get(res, "(au)", &array);

        if (g_variant_iter_n_children(array) == 0)
        {
            std::cerr << "-- ERROR -- | empty result of t_QueueCheck() |" << std::endl;
            return 1;
        }

        GVariant *e = nullptr;
        while ((e = g_variant_iter_next_value(array)))
        {
            unsigned int id = g_variant_get_uint32(e);
            try
            {
                GVariant *req = proxy.Call("t_QueueFetch",
                                           g_variant_new("(uuu)",
                                                         ClientAttentionType::CREDENTIALS,
                                                         (ClientAttentionGroup) group,
                                                         id));
                struct RequiresSlot reqdata;
                deserialize(reqdata, req);
                dump_requires_slot(reqdata, id);

                reqdata.value = "generated-data_" + reqdata.name +"_" + std::to_string(reqdata.id);
                proxy.Call("t_ProvideResponse",
                           g_variant_new("(uuus)",
                                         reqdata.type,
                                         reqdata.group,
                                         reqdata.id,
                                         reqdata.value.c_str()));
                g_variant_unref(req);
            }
            catch (DBusException &excp)
            {
                std::cerr << "-- ERROR -- |" << excp.GetRawError() << "|" << std::endl;
                return 1;
            }
            g_variant_unref(e);
        }
        g_variant_iter_free(array);
        proxy.Call("ServerDumpResponse", true);
    }

    return 0;
}
#endif
