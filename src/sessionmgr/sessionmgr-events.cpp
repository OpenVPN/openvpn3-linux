//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   sessionmgr-events.cpp
 *
 * @brief  Implementation of SessionManager::Event signalling class
 */

#include <gio/gio.h>
#include <gdbuspp/glib2/utils.hpp>

#include "sessionmgr-events.hpp"


namespace SessionManager {

Event::Event(const std::string &path,
             EventType type,
             uid_t owner)
    : path(path), type(type), owner(owner)
{
}


Event::Event(GVariant *params)
{
    std::string g_type(g_variant_get_type_string(params));
    if ("(oqu)" != g_type)
    {
        throw SessionManager::Exception("Invalid data type for SessionManager::Event()");
    }

    path = glib2::Value::Extract<std::string>(params, 0);
    type = glib2::Value::Extract<SessionManager::EventType>(params, 1);
    owner = glib2::Value::Extract<uid_t>(params, 2);

    if (type > EventType::SESS_DESTROYED
        || type < EventType::SESS_CREATED)
    {
        throw SessionManager::Exception("Invalid SessionManager::EventType value");
    }
}


bool Event::empty() const noexcept
{
    return path.empty()
           && (EventType::UNSET == type)
           && 65535 == owner;
}


GVariant *Event::GetGVariant() const noexcept
{
    GVariantBuilder *b = glib2::Builder::Create("(oqu)");
    glib2::Builder::Add(b, path, "o");
    glib2::Builder::Add(b, type);
    glib2::Builder::Add(b, owner);
    return glib2::Builder::Finish(b);
}


const std::string Event::TypeStr(const EventType type,
                                 bool tech_form) noexcept
{
    switch (type)
    {
    case EventType::UNSET:
        return "[UNSET]";

    case EventType::SESS_CREATED:
        return (tech_form ? "SESS_CREATED" : "Session created");

    case EventType::SESS_DESTROYED:
        return (tech_form ? "SESS_DESTROYED" : "Session destroyed");
    }
    return "[UNKNOWN]";
}


const DBus::Signals::SignalArgList Event::SignalDeclaration(bool with_session_token) noexcept
{
    return {{"path", glib2::DataType::DBus<DBus::Object::Path>()},
            {"type", glib2::DataType::DBus<SessionManager::EventType>()},
            {"owner", glib2::DataType::DBus<uint32_t>()}};
}


bool Event::operator==(const Event &compare) const
{
    return ((compare.type == (const SessionManager::EventType)type)
            && (compare.owner == owner)
            && (0 == compare.path.compare(path)));
}


bool Event::operator!=(const Event &compare) const
{
    return !(this->operator==(compare));
}

} // namespace SessionManager
