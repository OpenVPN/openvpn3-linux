//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

/**
 * @file   proxy-devposture.hpp
 *
 * @brief  Provides a C++ object implementation of a D-Bus devposture
 *         object. This proxy class will perform the D-Bus calls and
 *         provide the results as native C++ data types.
 */

#pragma once

#include <iostream>
#include <memory>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include "build-config.h"

#ifdef HAVE_TINYXML
#include <tinyxml2.h>
#include <openvpn/common/exception.hpp>
#include <openvpn/common/xmlhelper.hpp>
using XmlDocPtr = std::shared_ptr<openvpn::Xml::Document>;
#endif

#include "dbus/requiresqueue-proxy.hpp"
#include "statistics.hpp"
#include "common/utils.hpp"
#include "events/status.hpp"
#include "log/log-helpers.hpp"
#include "log/dbus-log.hpp"


namespace DevPosture::Proxy {

class Exception : public std::exception
{
  public:
    Exception(const std::string &err) noexcept
        : error(err)
    {
    }
    ~Exception() noexcept = default;

    const char *what() const noexcept
    {
        return error.c_str();
    }

  private:
    const std::string error;
};



class Handler
{
  public:
    using Ptr = std::shared_ptr<Handler>;

    [[nodiscard]] static Ptr Create(DBus::Connection::Ptr conn)
    {
        return Ptr(new Handler(conn));
    }

#ifdef HAVE_TINYXML
    XmlDocPtr Introspect()
    {
        auto prxqry = DBus::Proxy::Utils::Query::Create(proxy);
        std::string introsp = prxqry->Introspect(Constants::GenPath("devposture"));
        XmlDocPtr doc;
        doc.reset(new openvpn::Xml::Document(introsp, "introspect"));
        return doc;
    }
#endif

    std::vector<DBus::Object::Path> GetRegisteredModules() const
    {
        using namespace std::string_literals;

        try
        {
            GVariant *r = proxy->Call(target, "GetRegisteredModules");

            return glib2::Value::ExtractVector<DBus::Object::Path>(r, 0);
        }
        catch (const DBus::Proxy::Exception &e)
        {
            throw Exception("Failed to retrieve registered modules: "s + e.what());
        }
    }

    std::string ProtocolLookup(const std::string &enterprise_id) const
    {
        using namespace std::string_literals;

        try
        {
            GVariant *r = proxy->Call(target,
                                      "ProtocolLookup",
                                      glib2::Value::CreateTupleWrapped(enterprise_id));

            return glib2::Value::Extract<std::string>(r, 0);
        }
        catch (const DBus::Proxy::Exception &e)
        {
            throw Exception("Error during protocol lookup: "s + e.what());
        }
    }

    std::string RunChecks(const std::string &protocol,
                          const std::string &request) const
    {
        using namespace std::string_literals;

        try
        {
            GVariantBuilder *args_builder = glib2::Builder::Create("(ss)");

            glib2::Builder::Add(args_builder, protocol);
            glib2::Builder::Add(args_builder, request);

            GVariant *r = proxy->Call(target,
                                      "RunChecks",
                                      glib2::Builder::Finish(args_builder));

            return glib2::Value::Extract<std::string>(r, 0);
        }
        catch (const DBus::Proxy::Exception &e)
        {
            throw Exception("Error while running checks: "s + e.what());
        }
    }

  private:
    DBus::Proxy::Client::Ptr proxy;
    DBus::Proxy::TargetPreset::Ptr target;

    Handler(DBus::Connection::Ptr conn)
        : proxy(DBus::Proxy::Client::Create(conn, Constants::GenServiceName("devposture"))),
          target(DBus::Proxy::TargetPreset::Create(Constants::GenPath("devposture"),
                                                   Constants::GenInterface("devposture")))
    {
        auto prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
        prxqry->CheckServiceAvail(Constants::GenServiceName("devposture"));

        // Delay the return up to 750ms, to ensure we have a valid
        // Device Posture service object available
        auto prxchk = DBus::Proxy::Utils::Query::Create(proxy);

        for (uint8_t i = 5; i > 0; i--)
        {
            if (prxchk->CheckObjectExists(target->object_path,
                                          target->interface))
            {
                break;
            }

            usleep(150000);
        }
    }
};

} // namespace DevPosture::Proxy
