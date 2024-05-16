//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

#pragma once

/**
 *  @file log/service-logger.hpp
 *
 *  @brief This is a wrapper around the LogWriter object to provide
 *         a logging API similar to what LogSender provides.  LogSender
 *         will send log events over a D-Bus connection.  But for the
 *         net.openvpn.v3.log, it is expected to receive such log events
 *         and log it to the log destination configured in the LogWriter.
 *
 *         Thus there is no API for the log service itself to write log
 *         entries in a simpler way than LogWriter + Events::Log.  This
 *         is the gap this LogService::Logger API fills
 */

#include "events/log.hpp"
#include "logfilter.hpp"
#include "logmetadata.hpp"
#include "logwriter.hpp"

namespace LogService {

class Logger
{
  public:
    using Ptr = std::shared_ptr<Logger>;

    [[nodiscard]] static Logger::Ptr Create(LogWriter::Ptr logwr,
                                            const LogGroup lgrp,
                                            Log::EventFilter::Ptr filter)
    {
        return Logger::Ptr(new Logger(logwr, lgrp, filter));
    }

    void Log(const Events::Log &logev,
             LogMetaData::Ptr metadata = nullptr,
             const bool duplicate_check = false);
    void Debug(const std::string &msg,
               LogMetaData::Ptr md = nullptr,
               const bool duplicate_check = false);
    void LogVerb2(const std::string &msg,
                  LogMetaData::Ptr md = nullptr,
                  const bool duplicate_check = false);
    void LogVerb1(const std::string &msg,
                  LogMetaData::Ptr md = nullptr,
                  const bool duplicate_check = false);
    void LogInfo(const std::string &msg,
                 LogMetaData::Ptr md = nullptr,
                 const bool duplicate_check = false);
    void LogWarn(const std::string &msg,
                 LogMetaData::Ptr md = nullptr,
                 const bool duplicate_check = false);
    void LogError(const std::string &msg, LogMetaData::Ptr md = nullptr);
    void LogCritical(const std::string &msg, LogMetaData::Ptr md = nullptr);
    void LogFATAL(const std::string &msg, LogMetaData::Ptr md = nullptr);

    LogWriter::Ptr GetLogWriter() const noexcept;

  private:
    LogWriter::Ptr logwr = nullptr;
    const LogGroup loggroup = LogGroup::UNDEFINED;
    Log::EventFilter::Ptr filter = nullptr;
    Events::Log last_log = {};

    Logger(LogWriter::Ptr logwr_,
           const LogGroup lgrp,
           Log::EventFilter::Ptr fltr);
};



inline Logger::Logger(LogWriter::Ptr logwr_,
                      const LogGroup lgrp,
                      Log::EventFilter::Ptr fltr)
    : logwr(logwr_), loggroup(lgrp), filter(fltr)
{
}


inline void Logger::Log(const Events::Log &logev,
                        LogMetaData::Ptr metadata,
                        const bool duplicate_check)
{
    if (duplicate_check && logev == last_log)
    {
        // If duplicate check is enabled, we skip this log event if
        // it's identical to the last previously logged event
        return;
    }

    if (filter && !filter->Allow(logev))
    {
        // Only perform the logging if the log event is within the log level
        // scope of this logger service
        return;
    }

    if (metadata)
    {
        logwr->AddMetaCopy(metadata);
    }

    logwr->Write(logev);
    last_log = logev;
}


inline void Logger::Debug(const std::string &msg,
                          LogMetaData::Ptr md,
                          const bool duplicate_check)
{
    Log(Events::Log(loggroup, LogCategory::DEBUG, msg), md, duplicate_check);
}


inline void Logger::LogVerb2(const std::string &msg,
                             LogMetaData::Ptr md,
                             const bool duplicate_check)
{
    Log(Events::Log(loggroup, LogCategory::VERB2, msg), md, duplicate_check);
}


inline void Logger::LogVerb1(const std::string &msg,
                             LogMetaData::Ptr md,
                             const bool duplicate_check)
{
    Log(Events::Log(loggroup, LogCategory::VERB1, msg), md, duplicate_check);
}


inline void Logger::LogInfo(const std::string &msg,
                            LogMetaData::Ptr md,
                            const bool duplicate_check)
{
    Log(Events::Log(loggroup, LogCategory::INFO, msg), md, duplicate_check);
}


inline void Logger::LogWarn(const std::string &msg,
                            LogMetaData::Ptr md,
                            const bool duplicate_check)
{
    Log(Events::Log(loggroup, LogCategory::WARN, msg), md, duplicate_check);
}


inline void Logger::LogError(const std::string &msg, LogMetaData::Ptr md)
{
    Log(Events::Log(loggroup, LogCategory::ERROR, msg), md, false);
}


inline void Logger::LogCritical(const std::string &msg, LogMetaData::Ptr md)
{
    Log(Events::Log(loggroup, LogCategory::CRIT, msg), md, false);
}


inline void Logger::LogFATAL(const std::string &msg, LogMetaData::Ptr md)
{
    Log(Events::Log(loggroup, LogCategory::FATAL, msg), md, false);
    // TODO: Consider to throw a LogService exception here
}


inline LogWriter::Ptr Logger::GetLogWriter() const noexcept
{
    return logwr;
}

} // namespace LogService
