//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022         OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
//

/**
 *  @file  journal-log-parse.cpp
 *
 *  @brief Implementation for retreiving OpenVPN 3 Linux related
 *         log lines from the systemd-journald
 */



#include <algorithm>
#include <iostream>
#include <vector>
#include <ctime>

#include "journal-log-parse.hpp"
#include "log-helpers.hpp"


namespace Log {
namespace Journald {


//
//  Log::Journald::LogEntry
//

LogEntry::LogEntry(sd_journal *journal)
{
    realtime = extract_journal_tstamp(journal);
    timestamp = timestamp_to_str(realtime);
    sender = extract_journal_field(journal, "O3_SENDER");
    interface = extract_journal_field(journal, "O3_INTERFACE");
    method = extract_journal_field(journal, "O3_METHOD");
    property = extract_journal_field(journal, "O3_PROPERTY");
    object_path = extract_journal_field(journal, "O3_OBJECT_PATH");
    logtag = extract_journal_field(journal, "O3_LOGTAG");
    pid = extract_journal_field(journal, "_PID");
    event = LogEvent(extract_journal_field(journal, "O3_LOG_GROUP"),
                     extract_journal_field(journal, "O3_LOG_CATEGORY"),
                     extract_journal_field(journal, "O3_SESSION_TOKEN"),
                     extract_journal_field(journal, "MESSAGE"));
}


const Json::Value LogEntry::GetJSON() const
{
    Json::Value ret;
    ret["TIMESTAMP"] = Json::Value::UInt64(realtime);
    ret["TIMESTAMP_STRING"] = timestamp;
    ret["PID"] = pid;

    if (!sender.empty())
    {
        ret["O3_SENDER"] = sender;
    }
    if (!interface.empty())
    {
        ret["O3_INTERFACE"] = interface;
    }
    if (!method.empty())
    {
        ret["O3_METHOD"] = method;
    }
    if (!property.empty())
    {
        ret["O3_PROPERTY"] = property;
    }
    if (!object_path.empty())
    {
        ret["O3_OBJECT_PATH"] = object_path;
    }
    if (!logtag.empty())
    {
        ret["O3_LOGTAG"] = logtag;
    }
    if (!event.empty())
    {
        Json::Value logev;

        logev["LOG_GROUP"] = (uint8_t)event.group;
        logev["LOG_GROUP_STRING"] = event.GetLogGroupStr();
        logev["LOG_CATEGORY"] = (uint8_t)event.category;
        logev["LOG_CATEGORY_STRING"] = event.GetLogCategoryStr();

        if (!event.session_token.empty())
        {
            logev["SESSION_TOKEN"] = event.session_token;
        }

        std::stringstream msg;
        std::string line;
        msg << event.message;
        while (std::getline(msg, line, '\n'))
        {
            logev["LOG_MESSAGE"].append(line);
        }
        ret["LOG_EVENT"] = logev;
    }
    return ret;
}


std::string LogEntry::extract_journal_field(sd_journal *journal, const std::string &field) const
{
    int r = -1;
    const char *data = NULL;
    size_t l = 0;

    r = sd_journal_get_data(journal, field.c_str(), (const void **)&data, &l);
    if (r < 1 && data)
    {
        int skip = field.length() + 1;
        return std::string(&data[skip]);
    }
    return std::string();
}


uint64_t LogEntry::extract_journal_tstamp(sd_journal *journal) const
{
    std::string tstamp = extract_journal_field(journal, "_SOURCE_REALTIME_TIMESTAMP");
    if (tstamp.empty())
    {
        return -1;
    };

    return ::atol(tstamp.c_str());
}


const std::string LogEntry::timestamp_to_str(uint64_t tstmp) const
{
    if (tstmp < 0)
    {
        return "";
    }
    // The timestamp granularity in the journal is microseconds.
    // We don't need that kind of granularity and it is easier to
    // just use the normal time_t type
    ::time_t t = tstmp / 1000000;
    std::string ret(std::ctime(&t));
    ret.erase(ret.find_last_not_of("\n") + 1);
    return ret;
}



//
//  Log::Journald::LogEntries
//

const Json::Value LogEntries::GetJSON() const
{
    if (size() < 1)
    {
        return Json::Value(Json::arrayValue);
    }

    Json::Value ret;
    for (const auto &ev : *this)
    {
        ret.append(ev.GetJSON());
    }
    return ret;
}


//
//  Log::Journald::Parse::Exception
//

Parse::Exception::Exception(const std::string &err)
    : errmsg(std::move(err))
{
}


Parse::Exception::Exception()
    : errmsg()
{
}


const char *Parse::Exception::what() const noexcept
{
    return errmsg.c_str();
}



//
//  Log::Journald::Parse::FilterException
//

Parse::FilterException::FilterException(const std::string &err)
    : Exception(err)
{
}


Parse::FilterException::FilterException(const FilterType ft, const std::string &match)
    : Exception()
{
    std::string filter = {};
    switch (ft)
    {
    case FilterType::TIMESTAMP:
        filter = "timestamp";
        break;

    case FilterType::LOGTAG:
        filter = "logtag";
        break;

    case FilterType::SESSION_TOKEN:
        filter = "session token";
        break;

    case FilterType::OBJECT_PATH:
        filter = "D-Bus object path";
        break;

    case FilterType::SENDER:
        filter = "D-Bus sender";
        break;

    case FilterType::INTERFACE:
        filter = "D-Bus interface";
        break;

    default:
        filter = "UNKNOWN";
        break;
    }

    errmsg = "Error setting " + filter + " filter to '" + match + "'";
}



//
//  Log::Journald::Parse
//

Parse::Parse()
{
    int r = sd_journal_open(&journal, SD_JOURNAL_SYSTEM);
    if (r < 0)
    {
        throw Exception("Error calling sd_journal_open()");
    }
}


Parse::~Parse() noexcept
{
    sd_journal_close(journal);
}


void Parse::AddFilter(const FilterType ft, const std::string &fval) const
{
    std::stringstream match;
    int r = 0;
    switch (ft)
    {
    case FilterType::TIMESTAMP:
        {
            match << fval;
            std::tm t = {};
            match >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
            // The journald granularity is microseconds, not seconds
            uint64_t seek_point = ::mktime(&t) * 1000000;
            r = sd_journal_seek_realtime_usec(journal, seek_point);
            if (r < 0)
            {
                throw FilterException(ft, fval);
            }
            if (sd_journal_next(journal) < 0)
            {
                throw FilterException("sd_journal_next() failed");
            }
        }
        // The timestamp based filtering is not an ordinary match,
        // as we just fast-forward in the journald log to the right
        // point in time.
        return;

    case FilterType::LOGTAG:
        match << "O3_LOGTAG=" << fval;
        break;

    case FilterType::SESSION_TOKEN:
        match << "O3_SESSION_TOKEN=" << fval;
        break;

    case FilterType::OBJECT_PATH:
        match << "O3_OBJECT_PATH=" << fval;
        break;

    case FilterType::SENDER:
        match << "O3_SENDER=" << fval;
        break;

    case FilterType::INTERFACE:
        match << "O3_INTERFACE=" << fval;
        break;

    default:
        throw FilterException("Unexpected filter type");
    }
    sd_journal_add_match(journal, match.str().c_str(), 0);
    sd_journal_add_conjunction(journal);
}


LogEntries Parse::Retrieve()
{
    LogEntries ret = {};

    //  These are the common identifiers OpenVPN 3 Linux logger service
    //  identifiers on Linux
    sd_journal_add_match(journal, "SYSLOG_IDENTIFIER=openvpn3-service-logger", 0);
    sd_journal_add_disjunction(journal);
    sd_journal_add_match(journal, "SYSLOG_IDENTIFIER=net.openvpn.v3.log", 0);

    while (sd_journal_next(journal) > 0)
    {
        ret.push_back(LogEntry(journal));
    }
    return ret;
}

} // namespace Journald
} // namespace Log
