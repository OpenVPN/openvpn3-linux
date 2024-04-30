//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   attention-req.hpp
 *
 * @brief  Provides the handling object for AttentionRequired signals
 */

#pragma once

#include <string>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "dbus/constants.hpp"


namespace Events {

struct AttentionReq
{
    ClientAttentionType type = ClientAttentionType::UNSET;
    ClientAttentionGroup group = ClientAttentionGroup::UNSET;
    std::string message = {};

    AttentionReq() = default;
    AttentionReq(const ClientAttentionType att_type,
                 const ClientAttentionGroup att_group,
                 const std::string &msg);
    AttentionReq(GVariant *params);
    ~AttentionReq() = default;

    static DBus::Signals::SignalArgList SignalDeclaration() noexcept;

    void reset();
    const bool empty() const;

    const std::string Type() const;
    const std::string Group() const;
    const std::string Message() const;

    GVariant *GetGVariant() const;

    friend std::ostream &operator<<(std::ostream &os, const AttentionReq &ev)
    {
        return os << "AttentionReq("
                  << ev.Type() << ", " << ev.Group() << "): " << ev.message;
    }
};

} // namespace Events
