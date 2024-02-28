//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   netcfg-subscriptions.cpp
 *
 * @brief  Manages NetCfgChangeEvent/NetworkChange signal subscriptions
 */

#include <cstdint>
#include <sstream>
#include <gdbuspp/credentials/exceptions.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/glib2/utils.hpp>

#include "netcfg-exception.hpp"
#include "netcfg-subscriptions.hpp"
#include "netcfg-signals.hpp"


NetCfgSubscriptions::NetCfgSubscriptions(std::shared_ptr<NetCfgSignals> signals_,
                                         DBus::Credentials::Query::Ptr creds_qry_)
    : signals(signals_), creds_query(creds_qry_)
{
}


void NetCfgSubscriptions::SubscriptionSetup(DBus::Object::Base *object_ptr,
                                            const std::string &name_subscribe,
                                            const std::string &name_unsubscribe,
                                            const std::string &name_list)
{
    if (!object_ptr)
    {
        throw DBus::Exception("NetCfgSubscription",
                              "DBus::Object::Base pointer is invalid");
    }

    auto args_subscribe = object_ptr->AddMethod(
        name_subscribe,
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_name_subscribe(args);
        });
    args_subscribe->AddInput("filter", glib2::DataType::DBus<NetCfgChangeType>());

    auto args_unsubscribe = object_ptr->AddMethod(
        name_unsubscribe,
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_name_unsubscribe(args);
        });
    args_unsubscribe->AddInput("optional_subscriber",
                               glib2::DataType::DBus<std::string>());

    auto args_list = object_ptr->AddMethod(
        name_list,
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_name_list(args);
        });
}


void NetCfgSubscriptions::Subscribe(const std::string &sender, uint32_t filter_flags)
{
    if (0 == filter_flags)
    {
        throw NetCfgException("Subscription filter flag must be > 0");
    }
    if (65535 < filter_flags)
    {
        throw NetCfgException("Invalid subscription flag, must be < 65535");
    }
    subscriptions[sender] = static_cast<uint16_t>(filter_flags);
}


void NetCfgSubscriptions::Unsubscribe(const std::string &subscriber)
{
    if (subscriptions.find(subscriber) == subscriptions.end())
    {
        throw NetCfgException("Subscription not found for '"
                              + subscriber + "'");
    }
    subscriptions.erase(subscriber);
}


GVariant *NetCfgSubscriptions::List()
{
    // Build up the array of subscriber and subscription flags
    GVariantBuilder *bld = glib2::Builder::Create("a(su)");
    if (nullptr == bld)
    {
        throw NetCfgException("System error preparing response "
                              "(NetCfgSubscriptions::List)");
    }

    for (const auto &sub : subscriptions)
    {
        g_variant_builder_add(bld, "(su)", sub.first.c_str(), sub.second);
    }
    return glib2::Builder::FinishWrapped(bld);
}


std::vector<std::string> NetCfgSubscriptions::GetSubscribersList(const NetCfgChangeEvent &ev) const
{
    // Loop through all subscribers and identify who wants this
    // notification.
    std::vector<std::string> targets;
    for (const auto &s : subscriptions)
    {
        if ((uint16_t)ev.type & s.second)
        {
            targets.push_back(s.first);
        }
    }
    return targets;
}


void NetCfgSubscriptions::method_name_subscribe(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    glib2::Utils::checkParams(__func__, params, "(u)", 1);
    std::string sender = args->GetCallerBusName();

    // Use a larger data type to allow better input validation
    uint32_t filter_flags = glib2::Value::Extract<uint32_t>(params, 0);
    Subscribe(sender, filter_flags);

    std::stringstream msg;
    msg << "New subscription: '" << sender << "' => "
        << NetCfgChangeEvent::FilterMaskStr(filter_flags, true);
    signals->LogVerb2(msg.str());
}


void NetCfgSubscriptions::method_name_unsubscribe(DBus::Object::Method::Arguments::Ptr args)
{
    // The main subscription key is the D-Bus caller's unique bus name
    std::string subscr_key = args->GetCallerBusName();

    //  By default, an external utility can only unsubscribe its
    //  own subscription while the root user accounts can
    //  unsubscribe any subscriber.
    //
    //  Who can call the unsubscribe methods is also managed by
    //  the D-Bus policy.  The default policy will only allow
    //  this by the openvpn and root user accounts.

    // Only retrieve the subscriber value if the call comes
    // from an admin user
    if (0 == creds_query->GetUID(subscr_key))
    {
        GVariant *params = args->GetMethodParameters();
        glib2::Utils::checkParams(__func__, params, "(s)");
        std::string unsub_key = glib2::Value::Extract<std::string>(params, 0);

        if (unsub_key.empty())
        {
            signals->LogCritical("Failed to retrieve subscriber");
            throw NetCfgException("Failed to retrieve subscriber");
        }
        subscr_key = unsub_key;
    }

    Unsubscribe(subscr_key);
    signals->LogVerb2("Unsubscribed subscription: '" + subscr_key + "'");
}


void NetCfgSubscriptions::method_name_list(DBus::Object::Method::Arguments::Ptr args)
{
    // Only allow this method to be accessible by root
    uid_t uid = creds_query->GetUID(args->GetCallerBusName());
    if (0 != uid)
    {
        // FIXME: Improve error handling and use a better matching exception path
        throw DBus::Credentials::Exception("NetCfgSubscription",
                                           "Access denied");
    }
    args->SetMethodReturn(List());
}
