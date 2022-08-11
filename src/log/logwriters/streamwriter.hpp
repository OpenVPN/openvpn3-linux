//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
    StreamLogWriter(std::ostream& dst);
    virtual ~StreamLogWriter();


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
    void Write(const std::string& data,
               const std::string& colour_init = "",
               const std::string& colour_reset = "") override;

protected:
    std::ostream& dest;
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
    ColourStreamWriter(std::ostream& dst, ColourEngine *ce);
    virtual ~ColourStreamWriter() = default;


    /*
     * Explicitly tells the compiler that we want to not to override an
     * existing Write and our Write with different arguments should not
     * generate a warning
     */
    using StreamLogWriter::Write;

    void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string& data) override;


private:
    ColourEngine *colours = nullptr;
};
