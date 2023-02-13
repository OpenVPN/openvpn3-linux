//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   requiresqueue.cpp
 *
 * @brief  A class which implements just the D-Bus server/service side of
 *         the RequiresQueue.  This provides a C++ API which will implements
 *         the introspection snippet generation, the D-Bus service methods
 *         in addition to the methods the service needs to prepare the
 *         RequiresQueue and retrieve the user responses sent from a
 *         front-end.
 */

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <exception>
#include <cassert>
#include <stdint.h>

#include "config.h"
#include "dbus/core.hpp"
#include "requiresqueue.hpp"



/*
 *  RequiresSlot
 */
RequiresSlot::RequiresSlot()
    : id(0),
      type(ClientAttentionType::UNSET),
      group(ClientAttentionGroup::UNSET),
      name(""), value(""), user_description(""),
      hidden_input(false), provided(false)
{
}



/*
 *  RequiresQueueException
 */
RequiresQueueException::RequiresQueueException(std::string err)
    : error(err),
      what_("[RequireQueryException] " + err)
{
}


RequiresQueueException::RequiresQueueException(std::string errname, std::string errmsg)
    : error(errmsg),
      errorname(errname),
      what_("[RequireQueryException] " + errmsg)
{
}


const char *RequiresQueueException::what() const noexcept
{
    return what_.c_str();
}


void RequiresQueueException::GenerateDBusError(GDBusMethodInvocation *invocation)
{
    std::string errnam = (!errorname.empty() ? errorname : "net.openvpn.v3.error.undefined");
    GError *dbuserr = g_dbus_error_new_for_dbus_error(errnam.c_str(), error.c_str());
    g_dbus_method_invocation_return_gerror(invocation, dbuserr);
    g_error_free(dbuserr);
}



/*
 *  RequiresQueue
 */

RequiresQueue::RequiresQueue()
    : reqids()
{
}


RequiresQueue::~RequiresQueue()
{
}


std::string RequiresQueue::IntrospectionMethods(const std::string &meth_qchktypegr,
                                                const std::string &meth_queuefetch,
                                                const std::string &meth_queuechk,
                                                const std::string &meth_provideresp)
{
    std::stringstream introspection;
    introspection << "    <method name='" << meth_qchktypegr << "'>"
                  << "      <arg type='a(uu)' name='type_group_list' direction='out'/>"
                  << "    </method>"
                  << "    <method name='" << meth_queuefetch << "'>"
                  << "      <arg type='u' name='type' direction='in'/>"
                  << "      <arg type='u' name='group' direction='in'/>"
                  << "      <arg type='u' name='id' direction='in'/>"
                  << "      <arg type='u' name='type' direction='out'/>"
                  << "      <arg type='u' name='group' direction='out'/>"
                  << "      <arg type='u' name='id' direction='out'/>"
                  << "      <arg type='s' name='name' direction='out'/>"
                  << "      <arg type='s' name='description' direction='out'/>"
                  << "      <arg type='b' name='hidden_input' direction='out'/>"
                  << "    </method>"
                  << "    <method name='" << meth_queuechk << "'>"
                  << "      <arg type='u' name='type' direction='in'/>"
                  << "      <arg type='u' name='group' direction='in'/>"
                  << "      <arg type='au' name='indexes' direction='out'/>"
                  << "    </method>"
                  << "    <method name='" << meth_provideresp << "'>"
                  << "      <arg type='u' name='type' direction='in'/>"
                  << "      <arg type='u' name='group' direction='in'/>"
                  << "      <arg type='u' name='id' direction='in'/>"
                  << "      <arg type='s' name='value' direction='in'/>"
                  << "    </method>";
    return introspection.str();
}


unsigned int RequiresQueue::RequireAdd(ClientAttentionType type,
                                       ClientAttentionGroup group,
                                       std::string name,
                                       std::string descr,
                                       bool hidden_input)
{
    struct RequiresSlot elmt;
    elmt.id = reqids[get_reqid_index(type, group)]++;
    elmt.type = type;
    elmt.group = group;
    elmt.name = name;
    elmt.user_description = descr;
    elmt.provided = false;
    elmt.hidden_input = hidden_input;
    slots.push_back(elmt);

    return elmt.id;
}


void RequiresQueue::QueueFetch(GDBusMethodInvocation *invocation,
                               GVariant *parameters)
{
    GLibUtils::checkParams(__func__, parameters, "(uuu)", 3);
    ClientAttentionType type = (ClientAttentionType)GLibUtils::ExtractValue<uint32_t>(parameters, 0);
    ClientAttentionGroup group = (ClientAttentionGroup)GLibUtils::ExtractValue<uint32_t>(parameters, 1);
    guint id = GLibUtils::ExtractValue<uint32_t>(parameters, 2);

    // Fetch the requested slot id
    for (auto &e : slots)
    {
        if (id == e.id)
        {
            if (e.type == type
                && e.group == group)
            {
                if (e.provided)
                {
                    throw RequiresQueueException("net.openvpn.v3.already-provided",
                                                 "User input already provided");
                }

                GVariant *elmt = g_variant_new("(uuussb)",
                                               e.type,
                                               e.group,
                                               e.id,
                                               e.name.c_str(),
                                               e.user_description.c_str(),
                                               e.hidden_input);
                g_dbus_method_invocation_return_value(invocation, elmt);
                return;
            }
        }
    }
    throw RequiresQueueException("net.openvpn.v3.element-not-found",
                                 "No requires queue element found");
}


void RequiresQueue::UpdateEntry(ClientAttentionType type,
                                ClientAttentionGroup group,
                                unsigned int id,
                                std::string newvalue)
{
    for (auto &e : slots)
    {
        if (e.type == type
            && e.group == (ClientAttentionGroup)group
            && e.id == id)
        {
            if (!e.provided)
            {
                e.provided = true;
                e.value = newvalue;
                return;
            }
            else
            {
                throw RequiresQueueException("net.openvpn.v3.error.input-already-provided",
                                             "Request ID " + std::to_string(id)
                                                 + " has already been provided");
            }
        }
    }
    throw RequiresQueueException("net.openvpn.v3.invalid-input",
                                 "No matching entry found in the request queue");
}


void RequiresQueue::UpdateEntry(GDBusMethodInvocation *invocation,
                                GVariant *indata)
{
    //
    // Typically used by the function parsing user provided data
    // usually a backend process who asked for user input
    //
    GLibUtils::checkParams(__func__, indata, "(uuus)", 4);
    ClientAttentionType type = (ClientAttentionType)GLibUtils::ExtractValue<uint32_t>(indata, 0);
    ClientAttentionGroup group = (ClientAttentionGroup)GLibUtils::ExtractValue<uint32_t>(indata, 1);
    guint id = GLibUtils::ExtractValue<uint32_t>(indata, 2);
    std::string value = GLibUtils::ExtractValue<std::string>(indata, 3);

    if (value.empty())
    {
        throw RequiresQueueException("net.openvpn.v3.error.invalid-input",
                                     "No value provided for RequiresSlot ID "
                                         + std::to_string(id));
    }

    UpdateEntry(type, group, id, value);
    g_dbus_method_invocation_return_value(invocation, NULL);
}


void RequiresQueue::ResetValue(ClientAttentionType type,
                               ClientAttentionGroup group,
                               unsigned int id)
{
    for (auto &e : slots)
    {
        if (e.type == type && e.group == group && e.id == id)
        {
            e.provided = false;
            e.value = "";
            return;
        }
    }
    throw RequiresQueueException("No matching entry found in the request queue");
}


std::string RequiresQueue::GetResponse(ClientAttentionType type,
                                       ClientAttentionGroup group,
                                       unsigned int id)
{
    for (auto &e : slots)
    {
        if (e.type == type && e.group == group && e.id == id)
        {
            if (!e.provided)
            {
                throw RequiresQueueException("Request never provided by front-end");
            }
            return e.value;
        }
    }
    throw RequiresQueueException("No matching entry found in the request queue");
}


std::string RequiresQueue::GetResponse(ClientAttentionType type,
                                       ClientAttentionGroup group,
                                       std::string name)
{
    for (auto &e : slots)
    {
        if (e.type == type && e.group == group && e.name == name)
        {
            if (!e.provided)
            {
                throw RequiresQueueException("Request never provided by front-end");
            }
            return e.value;
        }
    }
    throw RequiresQueueException("No matching entry found in the request queue");
}


unsigned int RequiresQueue::QueueCount(ClientAttentionType type,
                                       ClientAttentionGroup group)
{
    unsigned int ret = 0;
    for (auto &e : slots)
    {
        if (type == e.type && group == e.group)
        {
            ret++;
        }
    }
    return ret;
}


std::vector<RequiresQueue::ClientAttTypeGroup> RequiresQueue::QueueCheckTypeGroup()
{
    std::vector<RequiresQueue::ClientAttTypeGroup> ret;

    for (auto &e : slots)
    {
        if (!e.provided)
        {
            // Check if we've already spotted this type/group
            bool found = false;
            for (auto &r : ret)
            {
                ClientAttentionType t;
                ClientAttentionGroup g;
                std::tie(t, g) = r;
                if (t == e.type && g == e.group)
                {
                    // yes, we have
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ret.push_back(std::make_tuple(e.type, e.group));
            }
        }
    }
    return ret;
}


void RequiresQueue::QueueCheckTypeGroup(GDBusMethodInvocation *invocation)
{
    // Convert the std::vector to a GVariant based array GDBus can use
    // as the method call response
    std::vector<std::tuple<ClientAttentionType, ClientAttentionGroup>> qchk_res = QueueCheckTypeGroup();

    GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(uu)"));
    assert(NULL != bld);
    for (auto &e : qchk_res)
    {
        ClientAttentionType t;
        ClientAttentionGroup g;
        std::tie(t, g) = e;
        g_variant_builder_add(bld, "(uu)", (unsigned int)t, (unsigned int)g);
    }

    // Wrap the GVariant array into a tuple which GDBus expects
    GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
    g_variant_builder_add_value(ret, g_variant_builder_end(bld));
    g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(ret));

    // Clean-up GVariant builders
    g_variant_builder_unref(bld);
    g_variant_builder_unref(ret);
}


std::vector<unsigned int> RequiresQueue::QueueCheck(ClientAttentionType type,
                                                    ClientAttentionGroup group)
{
    std::vector<unsigned int> ret;
    for (auto &e : slots)
    {
        if (type == e.type
            && group == e.group
            && !e.provided)
        {
            ret.push_back(e.id);
        }
    }
    return ret;
}


void RequiresQueue::QueueCheck(GDBusMethodInvocation *invocation, GVariant *parameters)
{
    GLibUtils::checkParams(__func__, parameters, "(uu)", 2);
    ClientAttentionType type = (ClientAttentionType)GLibUtils::ExtractValue<uint32_t>(parameters, 0);
    ClientAttentionGroup group = (ClientAttentionGroup)GLibUtils::ExtractValue<uint32_t>(parameters, 1);

    // Convert the std::vector to a GVariant based array GDBus can use
    // as the method call response
    std::vector<unsigned int> qchk_result = QueueCheck(type, group);
    GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("au"));
    for (auto &e : qchk_result)
    {
        g_variant_builder_add(bld, "u", e);
    }

    // Wrap the GVariant array into a tuple which GDBus expects
    GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
    g_variant_builder_add_value(ret, g_variant_builder_end(bld));
    g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(ret));

    // Clean-up GVariant builders
    g_variant_builder_unref(bld);
    g_variant_builder_unref(ret);
}


unsigned int RequiresQueue::QueueCheckAll()
{
    unsigned int ret = 0;
    for (auto &e : slots)
    {
        if (!e.provided)
        {
            ret++;
        }
    }
    return ret;
}


bool RequiresQueue::QueueAllDone()
{
    return QueueCheckAll() == 0;
}


bool RequiresQueue::QueueDone(ClientAttentionType type, ClientAttentionGroup group)
{
    return QueueCheck(type, group).size() == 0;
}


bool RequiresQueue::QueueDone(GVariant *parameters)
{
    // First, grab the slot ID ...
    GLibUtils::checkParams(__func__, parameters, "(uuus)", 4);
    ClientAttentionType type = (ClientAttentionType)GLibUtils::ExtractValue<uint32_t>(parameters, 0);
    ClientAttentionGroup group = (ClientAttentionGroup)GLibUtils::ExtractValue<uint32_t>(parameters, 1);

    // Check if there are any elements needing attentions in that slot ID
    return QueueCheck(type, group).size() == 0;
}
