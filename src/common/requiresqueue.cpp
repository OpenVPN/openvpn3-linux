//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
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
#include <mutex>
#include <vector>
#include <algorithm>
#include <exception>
#include <cassert>
#include <stdint.h>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/object/base.hpp>

#include "build-config.h"
#include "requiresqueue.hpp"

using namespace DBus;

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
    : DBus::Exception("RequiresQueue", err)
{
}


RequiresQueueException::RequiresQueueException(std::string errname, std::string errmsg)
    : DBus::Exception("RequiresQueue", errname + ": " + errmsg)
{
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


void RequiresQueue::QueueSetup(DBus::Object::Base *object_ptr,
                               const std::string &meth_qchktypegr,
                               const std::string &meth_queuefetch,
                               const std::string &meth_queuechk,
                               const std::string &meth_provideresp)
{
    if (!object_ptr)
    {
        throw DBus::Exception("RequiresQueue", "DBus::Object::Base pointer is invalid");
    }

    auto chktype_gr = object_ptr->AddMethod(meth_qchktypegr,
                                            [this](DBus::Object::Method::Arguments::Ptr args)
                                            {
                                                auto r = this->QueueCheckTypeGroupGVariant();
                                                args->SetMethodReturn(r);
                                            });
    chktype_gr->AddOutput("type_group_list", "a(uu)");

    auto queue_fetch = object_ptr->AddMethod(meth_queuefetch,
                                             [this](DBus::Object::Method::Arguments::Ptr args)
                                             {
                                                 auto r = this->QueueFetchGVariant(args->GetMethodParameters());
                                                 args->SetMethodReturn(r);
                                             });
    queue_fetch->AddInput("type", "u");
    queue_fetch->AddInput("group", "u");
    queue_fetch->AddInput("id", "u");
    queue_fetch->AddOutput("type", "u");
    queue_fetch->AddOutput("group", "u");
    queue_fetch->AddOutput("id", "u");
    queue_fetch->AddOutput("name", "s");
    queue_fetch->AddOutput("descripiton", "s");
    queue_fetch->AddOutput("hidden_input", "b");

    auto queue_chk = object_ptr->AddMethod(meth_queuechk,
                                           [this](DBus::Object::Method::Arguments::Ptr args)
                                           {
                                               auto r = this->QueueCheckGVariant(args->GetMethodParameters());
                                               args->SetMethodReturn(r);
                                           });
    queue_chk->AddInput("type", "u");
    queue_chk->AddInput("group", "u");
    queue_chk->AddOutput("indexes", "au");


    auto prov_resp = object_ptr->AddMethod(meth_provideresp,
                                           [this](DBus::Object::Method::Arguments::Ptr args)
                                           {
                                               this->UpdateEntry(args->GetMethodParameters());
                                               args->SetMethodReturn(nullptr);
                                           });
    prov_resp->AddInput("type", "u");
    prov_resp->AddInput("group", "u");
    prov_resp->AddInput("id", "u");
    prov_resp->AddInput("value", "s");
}


void RequiresQueue::ClearAll() noexcept
{
    reqids.clear();
    slots.clear();
    try
    {
        slots.shrink_to_fit();
    }
    catch (...)
    {
        // We ignore errors in this case.
        // If this fails, we use spend a bit
        // more memory than strictly needed.
        std::cerr << "RequiresQueue::ClearAll() failed. Ignored." << std::endl;
    }
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


GVariant *RequiresQueue::QueueFetchGVariant(GVariant *parameters) const
{
    glib2::Utils::checkParams(__func__, parameters, "(uuu)", 3);
    ClientAttentionType type{static_cast<ClientAttentionType>(glib2::Value::Extract<uint32_t>(parameters, 0))};
    ClientAttentionGroup group{static_cast<ClientAttentionGroup>(glib2::Value::Extract<uint32_t>(parameters, 1))};
    guint id{glib2::Value::Extract<uint32_t>(parameters, 2)};

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
                return elmt;
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


void RequiresQueue::UpdateEntry(GVariant *indata)
{
    //
    // Typically used by the function parsing user provided data
    // usually a backend process who asked for user input
    //
    glib2::Utils::checkParams(__func__, indata, "(uuus)", 4);
    ClientAttentionType type = static_cast<ClientAttentionType>(glib2::Value::Extract<uint32_t>(indata, 0));
    ClientAttentionGroup group = static_cast<ClientAttentionGroup>(glib2::Value::Extract<uint32_t>(indata, 1));
    guint id = glib2::Value::Extract<uint32_t>(indata, 2);
    std::string value = glib2::Value::Extract<std::string>(indata, 3);

    if (value.empty())
    {
        throw RequiresQueueException("net.openvpn.v3.error.invalid-input",
                                     "No value provided for RequiresSlot ID "
                                         + std::to_string(id));
    }

    UpdateEntry(type, group, id, value);
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


const std::string RequiresQueue::GetResponse(ClientAttentionType type,
                                             ClientAttentionGroup group,
                                             unsigned int id) const
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


const std::string RequiresQueue::GetResponse(ClientAttentionType type,
                                             ClientAttentionGroup group,
                                             std::string name) const
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
                                       ClientAttentionGroup group) const noexcept
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


std::vector<RequiresQueue::ClientAttTypeGroup> RequiresQueue::QueueCheckTypeGroup() const noexcept
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


GVariant *RequiresQueue::QueueCheckTypeGroupGVariant() const noexcept
{
    // Convert the std::vector to a GVariant based array GDBus can use
    // as the method call response
    std::vector<std::tuple<ClientAttentionType, ClientAttentionGroup>> qchk_res = QueueCheckTypeGroup();

    GVariantBuilder *bld = glib2::Builder::Create("a(uu)");
    for (auto &e : qchk_res)
    {
        ClientAttentionType type;
        ClientAttentionGroup group;
        std::tie(type, group) = e;
        glib2::Builder::Add(bld,
                            g_variant_new("(uu)",
                                          static_cast<unsigned int>(type),
                                          static_cast<unsigned int>(group)));
    }
    return glib2::Builder::FinishWrapped(bld);
}


std::vector<unsigned int> RequiresQueue::QueueCheck(ClientAttentionType type,
                                                    ClientAttentionGroup group) const noexcept
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


GVariant *RequiresQueue::QueueCheckGVariant(GVariant *parameters) const noexcept
{
    glib2::Utils::checkParams(__func__, parameters, "(uu)", 2);
    ClientAttentionType type = static_cast<ClientAttentionType>(glib2::Value::Extract<uint32_t>(parameters, 0));
    ClientAttentionGroup group = static_cast<ClientAttentionGroup>(glib2::Value::Extract<uint32_t>(parameters, 1));

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

    GVariant *result = g_variant_builder_end(ret);

    // Clean-up GVariant builders
    g_variant_builder_unref(bld);
    g_variant_builder_unref(ret);

    return result;
}


bool RequiresQueue::QueueAllDone() const noexcept
{
    for (auto &e : slots)
    {
        if (!e.provided)
        {
            return false;
        }
    }
    return true;
}


bool RequiresQueue::QueueDone(ClientAttentionType type, ClientAttentionGroup group)
{
    return QueueCheck(type, group).size() == 0;
}


bool RequiresQueue::QueueDone(GVariant *parameters)
{
    // First, grab the slot ID ...
    glib2::Utils::checkParams(__func__, parameters, "(uuus)", 4);
    ClientAttentionType type = static_cast<ClientAttentionType>(glib2::Value::Extract<uint32_t>(parameters, 0));
    ClientAttentionGroup group = static_cast<ClientAttentionGroup>(glib2::Value::Extract<uint32_t>(parameters, 1));

    // Check if there are any elements needing attentions in that slot ID
    return QueueCheck(type, group).size() == 0;
}
