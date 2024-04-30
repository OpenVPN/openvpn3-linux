//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   backendstatus.cpp
 *
 * @brief  Implements the StatusEvent object, which includes a D-Bus GVariant
 *         value parser
 */

#include <cstdint>
#include <sstream>
#include <glib.h>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "dbus/constants.hpp"
#include "status.hpp"


namespace Events {

Status::Status(const StatusMajor maj, const StatusMinor min, const std::string &msg)
{
    reset();
    major = maj;
    minor = min;
    message = msg;
}

Status::Status()
{
    reset();
}


Status::Status(GVariant *status)
{
    reset();
    if (nullptr != status)
    {
        std::string g_type(g_variant_get_type_string(status));
        if ("(uus)" == g_type)
        {
            parse_tuple(status);
        }
        else if ("a{sv}" == g_type)
        {
            parse_dict(status);
        }
        else
        {
            throw DBus::Exception("StatusEvent", "Invalid status data");
        }
    }
}


DBus::Signals::SignalArgList Status::SignalDeclaration() noexcept
{
    return {{"code_major", glib2::DataType::DBus<StatusMajor>()},
            {"code_minor", glib2::DataType::DBus<StatusMinor>()},
            {"message", glib2::DataType::DBus<std::string>()}};
}


void Status::reset()
{
    major = StatusMajor::UNSET;
    minor = StatusMinor::UNSET;
    message.clear();
    print_mode = static_cast<uint8_t>(PrintMode::ALL);
#ifdef DEBUG_CORE_EVENTS
    show_numeric_status = true;
#else
    show_numeric_status = false;
#endif
}


bool Status::empty() const
{
    return (StatusMajor::UNSET == major)
           && (StatusMinor::UNSET == minor)
           && message.empty();
}


void Status::GetPrintMode(const Status::PrintMode m)
{
    print_mode = static_cast<unsigned short>(m);
}


bool Status::Check(const StatusMajor maj, const StatusMinor min) const
{
    return (maj == major) && (min == minor);
}


GVariant *Status::GetGVariantTuple() const
{
    return g_variant_new("(uus)",
                         static_cast<guint32>(major),
                         static_cast<guint32>(minor),
                         message.c_str());
}


GVariant *Status::GetGVariantDict() const
{
    GVariantBuilder *b = glib2::Builder::Create("a{sv}");
    g_variant_builder_add(b, "{sv}", "major", glib2::Value::Create(major));
    g_variant_builder_add(b, "{sv}", "minor", glib2::Value::Create(minor));
    g_variant_builder_add(b, "{sv}", "status_message", glib2::Value::Create(message));
    return glib2::Builder::Finish(b);
}


bool Status::operator==(const Status &compare) const
{
    return ((compare.major == (const StatusMajor)major)
            && (compare.minor == (const StatusMinor)minor)
            && (0 == compare.message.compare(message)));
}


bool Status::operator!=(const Status &compare) const
{
    return !(this->operator==(compare));
}


void Status::parse_dict(GVariant *status)
{
    reset();
    major = glib2::Value::Dict::Lookup<StatusMajor>(status, "major");
    minor = glib2::Value::Dict::Lookup<StatusMinor>(status, "minor");
    message = glib2::Value::Dict::Lookup<std::string>(status, "status_message");
}


void Status::parse_tuple(GVariant *status)
{
    reset();
    major = glib2::Value::Extract<StatusMajor>(status, 0);
    minor = glib2::Value::Extract<StatusMinor>(status, 1);
    message = glib2::Value::Extract<std::string>(status, 2);
}

} // namespace Events
