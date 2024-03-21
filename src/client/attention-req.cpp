//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   attention-req.cpp
 *
 * @brief  Implementing the AttentionReq handling object
 */

#include <gdbuspp/glib2/utils.hpp>

#include "attention-req.hpp"


AttentionReq::AttentionReq(const ClientAttentionType att_type,
                           const ClientAttentionGroup att_group,
                           const std::string &msg)
    : type(att_type), group(att_group), message(msg)
{
}


AttentionReq::AttentionReq(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(uus)", 3);
    type = glib2::Value::Extract<ClientAttentionType>(params, 0);
    group = glib2::Value::Extract<ClientAttentionGroup>(params, 1);
    message = glib2::Value::Extract<std::string>(params, 2);
}


void AttentionReq::reset()
{
    type = ClientAttentionType::UNSET;
    group = ClientAttentionGroup::UNSET;
    message = {};
}


const bool AttentionReq::empty() const
{
    return (type == ClientAttentionType::UNSET)
           && (group == ClientAttentionGroup::UNSET)
           && message.empty();
}


const std::string AttentionReq::Type() const
{
    return ClientAttentionType_str[static_cast<uint32_t>(type)];
}


const std::string AttentionReq::Group() const
{
    return ClientAttentionGroup_str[static_cast<uint32_t>(group)];
}


const std::string AttentionReq::Message() const
{
    return message;
}


GVariant *AttentionReq::GetGVariant() const
{
    GVariantBuilder *b = glib2::Builder::Create("(uus)");
    glib2::Builder::Add(b, type);
    glib2::Builder::Add(b, group);
    glib2::Builder::Add(b, message);
    return glib2::Builder::Finish(b);
}
