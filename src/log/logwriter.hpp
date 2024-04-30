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
     * @param colour_init  std::string to be printed before log data, to
     *                     set the proper colours.  Emtpy by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Emtpy by default.
     *
     */
    virtual void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string &data,
                       const std::string &colour_init,
                       const std::string &colour_reset)
    {
        Write(LogPrefix(grp, ctg) + data, colour_init, colour_reset);
    }


    /**
     *  Writes log data to the destination buffer, but will prefix
     *  log lines with information about the log group and log category.
     *  This is a variant without pre/post string data
     *
     * @param grp      LogGroup the log message belongs to
     * @param ctg      LogCategory the log message is categorized as
     * @param data     std::string containing the log data
     */
    virtual void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string &data)
    {
        Write(grp, ctg, data, "", "");
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
            metadata.AddMeta(label, data, skip);
        }
    }


    /**
     *   Adds a LogTag value to the meta data variables in the log.
     *   Calling this method will always add the meta information,
     *   regardless if meta logging is enabled or not.
     *
     *  @param label   std::string of the label to use for this LogTag value
     *  @param tag     LogTag::Ptr to a LogTag object to use in the log
     */
    virtual void AddLogTag(const std::string &label, const LogTag::Ptr tag)
    {
        metadata.AddMeta(label, tag, true);
        PrependMeta("logtag", true);
    }


    void AddMetaCopy(const LogMetaData &mdc)
    {
        metadata = LogMetaData(mdc);
    }


    /**
     *  Puts a side-string from a meta data label which should be prepended to
     *  the next @Write() operation.  This is depends on a prior @AddMeta()
     *  call, where it uses the value set to a meta data label.  Only a single
     *  meta data label can be prepended.
     *
     *  @param label     std::string the meta data entry to prepend before @Write()
     *  @param prep_meta Bool indicating if this data should be prepended to
     *                   the meta log line as well.  This is reset on each
     *                   @Write() operation.
     */
    virtual void PrependMeta(const std::string &label, bool prep_meta = false)
    {
        prepend_label = label;
        prepend_meta = prep_meta;
    }


  protected:
    bool timestamp = true;
    bool log_meta = true;
    LogMetaData metadata;
    bool prepend_prefix = true;
    std::string prepend_label;
    bool prepend_meta = false;
};
