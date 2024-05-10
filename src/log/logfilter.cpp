//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//



#include "logfilter.hpp"


namespace Log {

EventFilter::Ptr EventFilter::Create(const uint32_t loglvl)
{
    return Ptr(new EventFilter(loglvl));
}


EventFilter::EventFilter(const uint32_t loglvl) noexcept
    : log_level(loglvl)
{
}


void EventFilter::SetLogLevel(const uint32_t loglev)
{
    if (loglev > 6)
    {
        throw LogException("LogSender: Invalid log level");
    }
    log_level = loglev;
}


uint32_t EventFilter::GetLogLevel() const noexcept
{
    return log_level;
}


void EventFilter::AddPathFilter(const DBus::Object::Path &path) noexcept
{
    filter_paths.push_back(path);
    std::sort(filter_paths.begin(), filter_paths.end());
}


bool EventFilter::Allow(const Events::Log &logev) const noexcept
{
    return Allow(logev.category);
}


bool EventFilter::Allow(const LogCategory catg) const noexcept
{
    switch (catg)
    {
    case LogCategory::DEBUG:
        return log_level >= 6;
    case LogCategory::VERB2:
        return log_level >= 5;
    case LogCategory::VERB1:
        return log_level >= 4;
    case LogCategory::INFO:
        return log_level >= 3;
    case LogCategory::WARN:
        return log_level >= 2;
    case LogCategory::ERROR:
        return log_level >= 1;
    default:
        return true;
    }
}


bool EventFilter::AllowPath(const DBus::Object::Path &path) const noexcept
{
    if (filter_paths.size() < 1)
    {
        return true;
    }
    auto r = std::lower_bound(filter_paths.begin(), filter_paths.end(), path);
    return r != filter_paths.end();
}


} // namespace Log
