//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP
#define OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP

#include "dbus/core.hpp"
#include "common/requiresqueue.hpp"

using namespace openvpn;

class DBusRequiresQueueProxy : public DBusProxy
{
public:
    DBusRequiresQueueProxy(GBusType bus_type, std::string destination , std::string interface, std::string objpath,
                           std::string method_quechktypegroup, std::string method_queuefetch, std::string method_queuecheck, std::string method_providereponse)
        : DBusProxy(bus_type, destination, interface, objpath),
          method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
    }
    DBusRequiresQueueProxy(DBus & dbusobj, std::string destination , std::string interface, std::string objpath,
                           std::string method_quechktypegroup, std::string method_queuefetch, std::string method_queuecheck, std::string method_providereponse)
        : DBusProxy(dbusobj, destination, interface, objpath),
          method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
    }

    void QueueFetch(struct RequiresSlot& destslot, ClientAttentionType type, ClientAttentionGroup group, unsigned int id)
    {
        GVariant *slot = Call(method_queuefetch, g_variant_new("(uuu)", type, group, id));
        if (NULL == slot)
        {
            THROW_DBUSEXCEPTION("DBusRequiresQueueProxy", "Failed during call to QueueFetch()");
        }

        deserialize(destslot, slot);
        g_variant_unref(slot);
    }

    void QueueFetchAll(std::vector<struct RequiresSlot>& slots, ClientAttentionType type, ClientAttentionGroup group)
    {
        std::vector<unsigned int> reqids = QueueCheck(type, group);
        for (auto& id : reqids)
        {
            struct RequiresSlot slot = {0};
            QueueFetch(slot, type, group, id);
            slots.push_back(std::move(slot));
        }
    }


    std::vector<RequiresQueue::ClientAttTypeGroup> QueueCheckTypeGroup()
    {
        GVariant *res = Call(method_quechktypegroup);
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("DBusRequiresQueueProxy",
                                "Failed during call to QueueCheckTypeGroup()");
        }

        GVariantIter *ar_type_group = NULL;
        g_variant_get(res, "(a(uu))", &ar_type_group);

        GVariant *e = NULL;
        std::vector<RequiresQueue::ClientAttTypeGroup> ret;
        while ((e = g_variant_iter_next_value(ar_type_group)))
        {
            unsigned int t;
            unsigned int g;
            g_variant_get(e, "(uu)", &t, &g);
            ret.push_back(std::make_tuple((ClientAttentionType) t, (ClientAttentionGroup) g));
            g_variant_unref(e);
        }
        g_variant_unref(res);
        g_variant_iter_free(ar_type_group);
        return ret;
    }

    std::vector<unsigned int> QueueCheck(ClientAttentionType type, ClientAttentionGroup group)
    {
        GVariant *res = Call(method_queuecheck, g_variant_new("(uu)", type, group));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("DBusRequiresQueueProxy", "Failed during call to QueueCheck()");
        }

        GVariantIter *slots = NULL;
        g_variant_get(res, "(au)", &slots);

        GVariant *e = NULL;
        std::vector<unsigned int> ret;
        while ((e = g_variant_iter_next_value(slots)))
        {
            ret.push_back(g_variant_get_uint32(e));
            g_variant_unref(e);
        }
        g_variant_unref(res);
        g_variant_iter_free(slots);
        return ret;
    }

    void ProvideResponse(struct RequiresSlot& slot)
    {
        GVariant *res = Call(method_provideresponse, g_variant_new("(uuus)",
                    slot.type, slot.group, slot.id, slot.value.c_str()));
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("DBusRequiresQueueProxy", "Failed during call to QueueCheck()");
        }
        g_variant_unref(res);
    }


private:
    std::string method_quechktypegroup;
    std::string method_queuefetch;
    std::string method_queuecheck;
    std::string method_provideresponse;

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
            if (result.provided || !result.value.empty())
            {
                throw RequiresQueueException("RequiresSlot destination is not empty/unused");
            }

            gchar *name = nullptr;
            gchar *descr = nullptr;
            g_variant_get(indata, "(uuussb)",
                          &result.type,
                          &result.group,
                          &result.id,
                          &name,
                          &descr,
                          &result.hidden_input);
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
            throw RequiresQueueException("Unknown input data formatting ");
        }
    }
};

#endif // OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP
