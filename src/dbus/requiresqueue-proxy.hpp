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
 * @file   requiresqueue-proxy.hpp
 *
 * @brief  A class which implements just the D-Bus client/proxy side of
 *         the RequiresQueue.  This provides a C++ API which will perform the
 *         appropriate D-Bus method calls against a service implementing
 *         a RequiresQueue.
 */

#ifndef OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP
#define OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP

#include "dbus/core.hpp"
#include "common/requiresqueue.hpp"

using namespace openvpn;


/**
 *  Main RequiresQueue proxy class, which implements the C++ API used to
 *  access a RequiresQueue API on a D-Bus service also implementing the
 *  RequiresQueue.
 */
class DBusRequiresQueueProxy : public DBusProxy
{
public:
    /**
     *  Initialize the D-Bus proxy for RequiresQueue.  This constructor
     *  will setup its own connection object.
     *
     * @param bus_type                 Defines if the connection is against
     *                                 the system or session bus.
     * @param destination              String containing the service
     *                                 destination to connect against
     * @param interface                String containing the D-Bus object's
     *                                 interface to use
     * @param objpath                  String containing the D-Bus object
     *                                 path to operate on
     * @param method_quechktypegroup   String containing the name of the
     *                                 QueueCheckTypeGroup method
     * @param method_queuefetch        String containing the name of the
     *                                 QueueFetch method
     * @param method_queuecheck        String containing the name of the
     *                                 QueueCheck method
     * @param method_providereponse    String containing the name of the
     *                                 QueueProvideResponse method
     *
     * The method names must match the defined introspection of the service
     * side.
     */
    DBusRequiresQueueProxy(GBusType bus_type, std::string destination , std::string interface, std::string objpath,
                           std::string method_quechktypegroup, std::string method_queuefetch, std::string method_queuecheck, std::string method_providereponse)
        : DBusProxy(bus_type, destination, interface, objpath),
          method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
    }


    /**
     *  Initialize the D-Bus proxy for RequiresQueue.  This constructor
     *  will re-use an established connection in a DBus object.
     *
     * @param dbusobj
     * @param destination              String containing the service
     *                                 destination to connect against
     * @param interface                String containing the D-Bus object's
     *                                 interface to use
     * @param objpath                  String containing the D-Bus object
     *                                 path to operate on
     * @param method_quechktypegroup   String containing the name of the
     *                                 QueueCheckTypeGroup method
     * @param method_queuefetch        String containing the name of the
     *                                 QueueFetch method
     * @param method_queuecheck        String containing the name of the
     *                                 QueueCheck method
     * @param method_providereponse    String containing the name of the
     *                                 QueueProvideResponse method
     *
     * The method names must match the defined introspection of the service
     * side.
     */
    DBusRequiresQueueProxy(DBus & dbusobj, std::string destination , std::string interface, std::string objpath,
                           std::string method_quechktypegroup, std::string method_queuefetch, std::string method_queuecheck, std::string method_providereponse)
        : DBusProxy(dbusobj.GetConnection(), destination, interface, objpath),
          method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
    }


    /**
     *  C++ method to use to fetch a specific RequiresQueue slot.
     *
     * @param destslot   The return buffer where the retrieved information
     *                   will be saved.  Must be pre-allocated before calling
     *                   this method.
     * @param type       ClientAttentionType of the slot to retrieve
     * @param group      ClientAttentionGroup of the slot to retrieve
     * @param id         Unsigned int of the slot ID to retrieve
     */
    struct RequiresSlot QueueFetch(ClientAttentionType type, ClientAttentionGroup group, unsigned int id)
    {
        GVariant *slot = Call(method_queuefetch, g_variant_new("(uuu)", type, group, id));
        if (NULL == slot)
        {
            THROW_DBUSEXCEPTION("DBusRequiresQueueProxy", "Failed during call to QueueFetch()");
        }

        struct RequiresSlot ret = deserialize(slot);
        g_variant_unref(slot);
        return ret;
    }


    /**
     *  C++ method used to retrieve all unresolved RequiresSlot records of a
     *  specific ClientAttentionType and ClientAttentionGroup.
     *
     * @param slots  std::vector<RequiresSlot> return buffer.  The initial
     *               vector must be pre-allocated.
     * @param type   ClientAttentionType to retrieve
     * @param group  ClientAttentionGroup to retrieve
     */
    void QueueFetchAll(std::vector<struct RequiresSlot>& slots, ClientAttentionType type, ClientAttentionGroup group)
    {
        std::vector<unsigned int> reqids = QueueCheck(type, group);
        for (auto& id : reqids)
        {
            slots.push_back(QueueFetch(type, group, id));
        }
    }


    /**
     *  Retrieves a RequiresQueue::ClientAttTypeGroup tuple containing all
     *  unresolved ClientAttentionTypes and ClientAttentionGroups.
     *
     * @return  Returns a std::vector<RequiresQueue::ClientAttTypeGroup> of
     *          unresolved types/groups.
     */
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


    /**
     *  Retrieves an array of slot IDs of unresolved items for a specific
     *  ClientAttentionType and ClientAttentionGroup.  An unresolved item
     *  is an item where the front-end has not provided any information yet.
     *
     * @param type    ClientAttentionType to retrieve items for
     * @param group   ClientAttentionGroup to retrieve items for
     * @return  Returns a std::vector<unsigned int> with slot ID references
     *          to unresolved RequiresSlot items.
     */
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


    /**
     *  Provides a response from the front-end to a specific RequiresSlot.
     *
     * @param slot  A RequiresSlot item containing the needed information.
     */
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


    /**
     *  Converts a D-Bus response for a RequiresSlot to a struct RequiresSlot
     *  item.
     *
     * @param result  The destination RequiresSlot where the data is to be put
     * @param indata  A GVariant object containing the response from a D-Bus
     *                call.  The type string is strict and needs to match
     *                between the sender (D-Bus service) and the receiver
     *                (this class).
     */
    struct RequiresSlot deserialize(GVariant *indata)
    {
        struct RequiresSlot result;

        if (!indata)
        {
            throw RequiresQueueException("indata GVariant pointer is NULL");
        }

        std::string data_type = std::string(g_variant_get_type_string(indata));
        if ("(uuussb)" == data_type)
        {
            guint32 type = 0;
            guint32 group;
            guint32 id;
            gchar *name = nullptr;
            gchar *descr = nullptr;
            gboolean hidden_input;
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
                result.name = std::string(name);
                g_free(name);
            }
            if (descr)
            {
                result.user_description = std::string(descr);
                g_free(descr);
            }
            result.hidden_input = hidden_input;
            return result;
        }
        throw RequiresQueueException("Failed parsing the requires queue result");
    }
};

#endif // OPENVPN3_DBUS_PROXY_REQUIRESQUEUE_HPP
