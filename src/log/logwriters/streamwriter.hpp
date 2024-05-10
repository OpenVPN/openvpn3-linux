//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   streamwriter.hpp
 *
 * @brief  Declaration of StreamLogWriter and ColourStreamWriter
 *         implementations of LogWriter
 */

#pragma once

#include <iostream>

#include "log/logwriter.hpp"
#include "log/colourengine.hpp"


/**
 *  LogWriter implementation, using std::ostream
 */
class StreamLogWriter : public LogWriter
{
  public:
    /**
     *  Initialize the StreamLogWriter
     *
     * @param dest  std::ostream to be used as the log destination
     *
     */
    StreamLogWriter(std::ostream &dst);
    virtual ~StreamLogWriter();

    const std::string GetLogWriterInfo() const override;

    /**
     *  Generic Write() method, which can allows prepended and appended
     *  data to encapsulate the log data.  This is used by the
     *  ColourStreamWriter to put colours on log events.
     *
     *  This implements LogWriter::Write(), using a std::ostream as the
     *  log destination
     *
     * @param data         std::string of data to be written
     * @param colour_init  std::string to be printed before log data, to
     *                     set the proper colours.  Empty by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Empty by default.
     */
    void Write(const std::string &data,
               const std::string &colour_init = "",
               const std::string &colour_reset = "") override;

  protected:
    std::ostream &dest;
};



/**
 *  Generic StreamLogWriter which makes the log output a bit more colourful.
 *  The colouring only applies when working on LogEvent() objects,
 *  otherwise it behaves similar to StreamLogWriter
 */
class ColourStreamWriter : public StreamLogWriter
{
  public:
    /**
     *  Initializes the colourful log writer
     *
     * @param dst  std::ostream to be used as the log destination
     * @param ce    ColourEngine object which provides knows how to
     *              do the proper colouring.
     */
    ColourStreamWriter(std::ostream &dst, ColourEngine *ce);
    virtual ~ColourStreamWriter() = default;

    const std::string GetLogWriterInfo() const override;

    /*
     * Explicitly tells the compiler that we want to not to override an
     * existing Write and our Write with different arguments should not
     * generate a warning
     */
    using StreamLogWriter::Write;

    /**
     *  Writes log data to the destination buffer, but will prefix
     *  log lines with information about the log group and log category
     *
     *  NOTE: The colour_init and colour_reset arguments are here for
     *        the override only; these values will be implicitly set
     *        by this Write() method using the ColourEngine.
     *
     * @param grp          LogGroup the log message belongs to
     * @param ctg          LogCategory the log message is categorized as
     * @param data         std::string containing the log data
     * @param colour_init  (ignored; this is set by ColourEngine)
     * @param colour_reset (ignored; this is set by ColourEngine)
     */
    void Write(const LogGroup grp,
               const LogCategory ctg,
               const std::string &data,
               const std::string &colour_init = "",
               const std::string &colour_reset = "") override;

  private:
    ColourEngine *colours = nullptr;
};
