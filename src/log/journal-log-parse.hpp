//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022-  David Sommerseth <davids@openvpn.net>
//

/**
 *  @file  journal-log-parse.cpp
 *
 *  @brief Declaration of the API used to query
 *         systemd-journald for OpenVPN 3 Linux log entries
 */

#pragma once

#include "build-config.h"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <json/json.h>

#include "events/log.hpp"

namespace Log {
namespace Journald {


/**
 *  Contains a single parsed log entry from the systemd-journald
 *
 */
struct LogEntry
{
    /**
     * Construct a new LogEntry object
     *
     * @param journal  sd_journal pointer to the log event to parse
     */
    LogEntry(sd_journal *journal);
    virtual ~LogEntry() noexcept = default;


    /**
     *  Retrieve the parsed log entry as a Json::Value object
     *
     * @return const Json::Value
     */
    const Json::Value GetJSON() const;


    /**
     *  stream operator to extract the log entry as a formatted string
     *
     * @param os
     * @param entr
     * @return std::ostream&
     */
    friend std::ostream &operator<<(std::ostream &os, const LogEntry &entr)
    {
        if (entr.logtag.empty())
        {
            return os << entr.timestamp << "  " << entr.event.str(15);
        }
        else
        {
            return os << entr.timestamp
                      << "  {tag:" << entr.logtag << "} "
                      << entr.event.str(15);
        }
    }


    uint64_t realtime = 0;        //<  Timestamp in microseconds of the log event
    std::string timestamp = {};   //<  Timestamp in human readable format, based on current locale
    std::string sender = {};      //<  D-Bus sender of the log event, if present
    std::string interface = {};   //<  D-Bus interface of the log event producer, if present
    std::string method = {};      //<  D-Bus method related to the log event, if present
    std::string property = {};    //<  D-Bus object property related to the log event, if present
    std::string object_path = {}; //<  D-Bus object path of the log event producer, if present
    std::string int_method = {};  //<  Internal method doing the call
    std::string logtag = {};      //<  OpenVPN 3 Linux LogTag value of the log event producer, if present
    std::string pid = {};         //<  Process ID of the logger process
    Events::Log event = {};       //<  Reconstructed LogTag object of the log event


  private:
    /**
     *  Helper method to extract a single journald variable of the log event
     *
     * @param journal       sd_journal pointer to the log event
     * @param field         std::string of the journald field to extract
     * @return std::string  Returns a std::string of the field content if found, otherwise an empty string
     */
    std::string extract_journal_field(sd_journal *journal, const std::string &field) const;


    /**
     *  Removes the logtag string from the logmsg if it is found in the
     *  log message string. The input logmsg string may be modified.
     *
     * @param logtag  std::string containing the log tag string to search for
     * @param logmsg  std::string containing the log message
     * @return std::string The cleaned up log message without log tag
     */
    const std::string strip_logtag(const std::string &logtag, std::string &logmsg);

    /**
     *  Extract the epoch (in microseconds) of the log event
     *
     * @param journal    sd_journal pointer to the log event
     * @return uint64_t
     */
    uint64_t extract_journal_tstamp(sd_journal *journal) const;

    /**
     *  Converts the journal microsecond epoch to a local time
     *  human readable string representation
     *
     * @param tstmp  uint64_t value of the microsecond epoch
     * @return const std::string
     */
    const std::string timestamp_to_str(uint64_t tstmp) const;
};



/**
 * Collection (std::vector) of LogEntry objects
 *
 * This class is instantiated by the Log::Journald::Parser::Retrieve() method
 */
class LogEntries : public std::vector<LogEntry>
{
  public:
    LogEntries() = default;
    virtual ~LogEntries() noexcept = default;

    /**
     *  Retrieve a JSON representation of all retrieved log entries
     *
     * @return const Json::Value
     */
    const Json::Value GetJSON() const;


    /**
     *  stream operator to retrieve all log entries as formatted strings
     *
     * @param os
     * @param entr
     * @return std::ostream&
     */
    friend std::ostream &operator<<(std::ostream &os, const LogEntries &entr)
    {
        for (const auto &e : entr)
        {
            os << e << std::endl;
        }
        return os;
    }
};



/**
 *  Main systemd-journald interface for retrieving and parsing log events
 */
class Parse
{
  public:
    /**
     *  Supported filter types to narrow down the log entry retrival
     */
    enum class FilterType : uint8_t
    {
        TIMESTAMP,
        LOGTAG,
        SESSION_TOKEN,
        OBJECT_PATH,
        SENDER,
        INTERFACE
    };



    /**
     *  Base Log::Journald::Parser exception
     *
     *  Used to throw generic error messages
     *
     */
    class Exception : public std::exception
    {
      public:
        Exception(const std::string &err);

        const char *what() const noexcept;

      protected:
        Exception();
        std::string errmsg = {};
    };



    /**
     *  Log::Journal::Parser exception used when there are
     *  issues with preparing log filter settings.
     *
     *  This is a subclass of Log::Journald:Parser::Exception
     */
    class FilterException : public Exception
    {
      public:
        FilterException(const std::string &err);
        FilterException(const FilterType ft, const std::string &match);
    };


    //
    //  Log::Journald::Parse
    //

    Parse();
    virtual ~Parse() noexcept;


    /**
     *  Adds a filter rule to the query.  Must be called before @Retrieve().
     *
     * @param ft     FilterType of value to filter
     * @param fval   std::string value to match against
     */
    void AddFilter(const FilterType ft, const std::string &fval) const;


    /**
     *  Retrieves the all the matching log entries related to OpenVPN 3 Linux.
     *  If no @AddFilter() calls has been done, all available records will
     * be extracted and parsed
     *
     * @return LogEntries
     */
    LogEntries Retrieve();

  private:
    sd_journal *journal = nullptr;
};


} // namespace Journald
} // namespace Log

#endif // HAVE_SYSTEMD
