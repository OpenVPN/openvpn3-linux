//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logwriter.hpp
 *
 * @brief  Base class declaration for a generic logging interface (API)
 *
 */

#pragma once

#include <syslog.h>

#include <memory>
#include <string>
#include <vector>

#include "build-config.h"
#include "events/log.hpp"
#include "logtag.hpp"
#include "logmetadata.hpp"


/**
 *  Base class providing a generic API for writing log data
 *  to an output stream
 */
class LogWriter
{
  public:
    using Ptr = std::shared_ptr<LogWriter>;

    LogWriter() = default;
    virtual ~LogWriter() = default;


    /**
     *  Retrieve information about the configured LogWriter backend in use
     *
     * @return  Returns a std::string with information about the backend
     */
    virtual const std::string GetLogWriterInfo() const = 0;


    /**
     *  Turns on/off prefixing log lines with the timestamp of the log event
     *
     * @param tstamp Boolean flag to enable (true) or disable (false)
     *               timestamps
     */
    virtual void EnableTimestamp(const bool tstamp) final
    {
        timestamp = tstamp;
    }


    virtual bool TimestampEnabled()
    {
        return timestamp;
    }


    /**
     *  Turns on/off logging meta data
     *
     * @param meta  Boolean to enable (true) or disable (false) the
     *              the meta data logging
     */
    void EnableLogMeta(const bool meta)
    {
        log_meta = meta;
    }


    bool LogMetaEnabled()
    {
        return log_meta;
    }


    void EnableMessagePrepend(const bool mp)
    {
        prepend_prefix = mp;
    }


    bool MessagePrependEnabled() const noexcept
    {
        return prepend_prefix;
    }


    /**
     *  Writes log data to the destination buffer
     *
     *  This is a pure virtual method which must be implemented.
     *
     * @param data         std::string of data to be written
     * @param colour_init  std::string to be printed before log data, to
     *                     set the proper colours.  Empty by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Empty by default.
     *
     */
    // clang-format off
    // bug in clang-format causes "= 0" to be wrapped

    virtual void Write(const std::string &data,
                       const std::string &colour_init = "",
                       const std::string &colour_reset = "") = 0;
    // clang-format on


    /**
     *  Writes log data to the destination buffer, but will prefix
     *  log lines with information about the log group and log category
     *
     * @param grp          LogGroup the log message belongs to
     * @param ctg          LogCategory the log message is categorized as
     * @param data         std::string containing the log data
     * @param colour_init  (optional) std::string to be printed before log
     *                     data, to set the proper colours.  Emtpy by default.
     * @param colour_reset (optional) std::string to be printed after the log
     *                     data to reset colour selection.  Emtpy by default.
     */
    virtual void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string &data,
                       const std::string &colour_init = "",
                       const std::string &colour_reset = "")
    {
        Write(LogPrefix(grp, ctg) + data, colour_init, colour_reset);
    }


    /**
     *  Writes a LogEvent() object in a formatted way.
     *
     * @param logev  Populated LogEvent() object to log
     *
     */
    virtual void Write(const Events::Log &logev)
    {
        Write(logev.group, logev.category, logev.message);
    }


    /**
     *  Adds meta log info, which is printed before the log line
     *  written by Write().  This must be added before each Write() call.
     *
     * @param label   std::string containing a label for this meta value
     * @param data    std::string containing meta data related to the log data
     * @param skip    bool flag if this value should be skipped/ignored in stream
     *                operations (default false)
     */
    virtual void AddMeta(const std::string &label,
                         const std::string &data,
                         bool skip = false)
    {
        if (log_meta)
        {
            metadata->AddMeta(label, data, skip);
        }
    }


    void AddMetaCopy(LogMetaData::Ptr mdc)
    {
        metadata = mdc->Duplicate();
    }


  protected:
    bool timestamp = true;
    bool log_meta = true;
    LogMetaData::Ptr metadata = nullptr;
    bool prepend_prefix = true;
};
