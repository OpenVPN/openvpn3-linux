//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   requiresqueue-proxy.hpp
 *
 * @brief  A class which implements just the D-Bus client/proxy side of
 *         the RequiresQueue.  This provides a C++ API which will perform the
 *         appropriate D-Bus method calls against a service implementing
 *         a RequiresQueue.
 */

#pragma once

#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>

#include "common/requiresqueue.hpp"


/**
 *  Main RequiresQueue proxy class, which implements the C++ API used to
 *  access a RequiresQueue API on a D-Bus service also implementing the
 *  RequiresQueue.
 */
class DBusRequiresQueueProxy
{
  public:
    /**
     *  Initialize the D-Bus proxy for RequiresQueue.  This constructor
     *  will re-use an established connection in a DBus object.
     *
     * @param dbuscon                  DBus::Connection::Ptr to use
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
    DBusRequiresQueueProxy(DBus::Connection::Ptr dbuscon,
                           const std::string &destination,
                           const std::string &interface,
                           const DBus::Object::Path &objpath,
                           const std::string &method_quechktypegroup,
                           const std::string &method_queuefetch,
                           const std::string &method_queuecheck,
                           const std::string &method_providereponse)
        : method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
        proxy = DBus::Proxy::Client::Create(dbuscon, destination);
        target = DBus::Proxy::TargetPreset::Create(objpath, interface);
    }


    /**
     *  This constructor will re-use an existing D-Bus proxy client for the
     *  communication towards the D-Bus service to query
     *
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
    DBusRequiresQueueProxy(const std::string &method_quechktypegroup,
                           const std::string &method_queuefetch,
                           const std::string &method_queuecheck,
                           const std::string &method_providereponse)
        : method_quechktypegroup(method_quechktypegroup),
          method_queuefetch(method_queuefetch),
          method_queuecheck(method_queuecheck),
          method_provideresponse(method_providereponse)
    {
    }


    /**
     *  If the DBusRequiresQueueProxy object was setup without a proxy,
     *  this method can be used to assign the proxy connection.
     *
     * @param prx                      DBus::Proxy::Client object to use for
     *                                 the D-Bus calls
     * @param tgt                      DBus::Proxy::TargetPreset object with the
     *                                 D-Bus object target specification
     * @return true if the operation was successful, otherwise false which
     *         means this object was already assigned to a proxy
     */
    const bool AssignProxy(DBus::Proxy::Client::Ptr prx,
                           DBus::Proxy::TargetPreset::Ptr tgt)
    {
        if (!prx && !target)
        {
            return false;
        }
        proxy = prx;
        target = tgt;
        return true;
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
    struct RequiresSlot QueueFetch(ClientAttentionType type,
                                   ClientAttentionGroup group,
                                   uint32_t id)
    {
        GVariant *slot = proxy->Call(target,
                                     method_queuefetch,
                                     g_variant_new("(uuu)", type, group, id));
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
    void QueueFetchAll(std::vector<struct RequiresSlot> &slots,
                       ClientAttentionType type,
                       ClientAttentionGroup group)
    {
        std::vector<uint32_t> reqids = QueueCheck(type, group);
        for (auto &id : reqids)
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
        GVariant *res = proxy->Call(target, method_quechktypegroup);
        GVariantIter *ar_type_group = nullptr;
        g_variant_get(res, "(a(uu))", &ar_type_group);

        GVariant *e = NULL;
        std::vector<RequiresQueue::ClientAttTypeGroup> ret;
        while ((e = g_variant_iter_next_value(ar_type_group)))
        {
            auto t{glib2::Value::Extract<ClientAttentionType>(e, 0)};
            auto g{glib2::Value::Extract<ClientAttentionGroup>(e, 1)};
            ret.push_back(std::make_tuple(t, g));
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
    std::vector<uint32_t> QueueCheck(ClientAttentionType type,
                                     ClientAttentionGroup group)
    {
        GVariant *res = proxy->Call(target,
                                    method_queuecheck,
                                    g_variant_new("(uu)", type, group));

        std::vector<uint32_t> ret = glib2::Value::ExtractVector<uint32_t>(res);
        return ret;
    }


    /**
     *  Provides a response from the front-end to a specific RequiresSlot.
     *
     * @param slot  A RequiresSlot item containing the needed information.
     */
    void ProvideResponse(struct RequiresSlot &slot)
    {
        GVariant *res = proxy->Call(target,
                                    method_provideresponse,
                                    g_variant_new("(uuus)",
                                                  slot.type,
                                                  slot.group,
                                                  slot.id,
                                                  slot.value.c_str()));
        g_variant_unref(res);
    }


  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr target = nullptr;
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
        if ("(uuussb)" != data_type)
        {
            throw RequiresQueueException("Failed parsing the requires queue result");
        }

        result.type = glib2::Value::Extract<ClientAttentionType>(indata, 0);
        result.group = glib2::Value::Extract<ClientAttentionGroup>(indata, 1);
        result.id = glib2::Value::Extract<uint32_t>(indata, 2);
        result.name = glib2::Value::Extract<std::string>(indata, 3);
        result.user_description = glib2::Value::Extract<std::string>(indata, 4);
        result.hidden_input = glib2::Value::Extract<bool>(indata, 5);
        return result;
    }
};
