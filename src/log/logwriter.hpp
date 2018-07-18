//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
 * @file   logwriter.hpp
 *
 * @brief  Base class implementing a generic logging interface (API)
 *         as well as file stream and syslog implementations.
 */

#pragma once

#include <syslog.h>

#include <fstream>
#include <exception>

#include "colourengine.hpp"
#include "log-helpers.hpp"
#include "dbus-log.hpp"


/**
 *  Base class providing a generic API for writing log data
 *  to an output stream
 */
class LogWriter
{
public:
    LogWriter()
    {
    }

    virtual ~LogWriter()
    {
    }


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


    /**
     *  Writes log data to the destination buffer
     *
     *  This is a pure virtual method which must be implemented.
     *
     * @param data         std::string of data to be written
     * @param colour_init  std::string to be printed before log data, to
     *                     set the proper colours.  Emtpy by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Emtpy by default.
     *
     */
    virtual void Write(const std::string& data,
                       const std::string& colour_init = "",
                       const std::string& colour_reset = "") = 0;

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
    virtual void Write(const LogGroup grp, const LogCategory ctg,
                       const std::string& data,
                       const std::string& colour_init,
                       const std::string& colour_reset)
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
    virtual void Write(const LogGroup grp, const LogCategory ctg,
                       const std::string& data)
    {
        Write(grp, ctg, data, "", "");
    }


    /**
     *  Writes a LogEvent() object in a formatted way.
     *
     * @param logev  Poplulated LogEvent() object to log
     *
     */
    virtual void Write(const LogEvent& logev)
    {
        Write(logev.group, logev.category, logev.message);
    }


    /**
     *  Adds meta log info, which is printed before the log line
     *  written by Write().  This must be added before each Write() call.
     *
     * @param data  std::string containing meta data related to the log data
     */
    virtual void AddMeta(const std::string& data)
    {
        metadata = data;
    }

protected:
    bool  timestamp = true;
    std::string metadata;
};



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
    StreamLogWriter(std::ostream& dest)
        : LogWriter(),
          dest(dest)
    {
    }

    virtual ~StreamLogWriter()
    {
        dest.flush();
    }


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
     *                     set the proper colours.  Emtpy by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Emtpy by default.
     */
    virtual void Write(const std::string& data,
                       const std::string& colour_init = "",
                       const std::string& colour_reset = "") override
    {
        if (!metadata.empty())
        {
            dest << (timestamp ? GetTimestamp() : "") << " "
                 << colour_init << metadata << colour_reset
                 << std::endl;
            metadata.clear();
        }
        dest << (timestamp ? GetTimestamp() : "") << " "
             << colour_init << data << colour_reset
             << std::endl;
    }

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
    enum class ColourMode : uint8_t {
        BY_GROUP,
        BY_CATEGORY
    };

    /**
     *  Initializes the colourful log writer
     *
     * @param dest  std::ostream to be used as the log destination
     * @param ce    ColourEngine object which provides knows how to
     *              do the proper colouring.
     */
    ColourStreamWriter(std::ostream& dest, ColourEngine *ce)
        : StreamLogWriter(dest), colours(ce)
    {
    }

    virtual ~ColourStreamWriter()
    {
    }


    /**
     *  Changes the colour mode.  This determins if the colour scheme
     *  to use should be based on the LogGroup (ColourMode::BY_GROUP) or by
     *  LogCategory (ColourMode::BY_CATEGORY).  The default is BY_CATEGORY.
     *
     * @param m  ColourMode to use
     *
     */
    void SetColourMode(ColourMode m)
    {
        mode = m;
    }

    virtual void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string& data) override
    {
        switch (mode)
        {
        case ColourMode::BY_CATEGORY:
            LogWriter::Write(grp, ctg, data,
                             colour_by_category(ctg),
                             colours->Reset());
            return;

        case ColourMode::BY_GROUP:
            {
                std::string grpcol = colour_by_group(grp);
                std::string ctgcol = (LogCategory::INFO < ctg ? colour_by_category(ctg) : grpcol);
                LogWriter::Write(grp, ctg, grpcol + data,
                                 ctgcol,
                                 colours->Reset());
            }
            break;

        default:
            LogWriter::Write(grp, ctg, data);
            return;
        }

    }


private:
    ColourEngine *colours = nullptr;
    ColourMode mode = ColourMode::BY_CATEGORY;


    /**
     *  Colour theme to use when mapping LogCategory to
     *  a colour to use for the log lines.
     *
     * @param ctg  LogCategory to colourize
     *
     * @return  Returns a std::string containing the proper colour code
     */
    const std::string colour_by_category(LogCategory ctg)
    {
        switch(ctg)
        {
        case LogCategory::DEBUG:
            return colours->Set(ColourEngine::Colour::BRIGHT_BLUE,
                                ColourEngine::Colour::NONE);

        case LogCategory::VERB2:
            return colours->Set(ColourEngine::Colour::BRIGHT_CYAN,
                                ColourEngine::Colour::NONE);

        case LogCategory::VERB1:
            return "";

        case LogCategory::INFO:
            return colours->Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::NONE);

        case LogCategory::WARN:
            return colours->Set(ColourEngine::Colour::BRIGHT_YELLOW,
                                ColourEngine::Colour::NONE);

        case LogCategory::ERROR:
            return colours->Set(ColourEngine::Colour::BRIGHT_RED,
                                ColourEngine::Colour::NONE);

        case LogCategory::CRIT:
            return colours->Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::RED);

        case LogCategory::FATAL:
            return colours->Set(ColourEngine::Colour::BRIGHT_YELLOW,
                                ColourEngine::Colour::RED);

        default:
            return "";
        }
    }


    /**
     *  Colour theme to use when mapping LogGroup to
     *  a colour to use for the log lines.
     *
     * @param ctg  LogGroupto colourize
     *
     * @return  Returns a std::string containing the proper colour code
     */
    const std::string colour_by_group(LogGroup grp)
    {
        switch(grp)
        {
        case LogGroup::CONFIGMGR:
            return colours->Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::GREEN);

        case LogGroup::SESSIONMGR:
            return colours->Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::BLUE);

        case LogGroup::BACKENDSTART:
            return colours->Set(ColourEngine::Colour::BRIGHT_WHITE,
                                ColourEngine::Colour::CYAN);

        case LogGroup::LOGGER:
            return colours->Set(ColourEngine::Colour::BRIGHT_GREEN,
                                ColourEngine::Colour::NONE);

        case LogGroup::BACKENDPROC:
            return colours->Set(ColourEngine::Colour::CYAN,
                                ColourEngine::Colour::NONE);

        case LogGroup::CLIENT:
            return colours->Set(ColourEngine::Colour::BRIGHT_YELLOW,
                                ColourEngine::Colour::NONE);

        case LogGroup::UNDEFINED:
        case LogGroup::MASTERPROC:
        default:
            return "";
        }
    }
};


/**
 *  LogWriter implementation, writing to syslog
 */
class SyslogWriter : public LogWriter
{
public:
    /**
     *  Initialize the SyslogWriter
     *
     * @param log_facility  Syslog facility to use for log messages.
     *                      (Default: LOG_DAEMON)
     */
    SyslogWriter(const char * progname = NULL,
                 const int log_facility = LOG_DAEMON)
        : LogWriter()
    {
        openlog(progname, LOG_NDELAY | LOG_PID, log_facility);
    }

    virtual ~SyslogWriter()
    {
        closelog();
    }


    virtual void Write(const std::string& data,
                       const std::string& colour_init = "",
                       const std::string& colour_reset = "")
    {
        // This is a very simple log implementation.  We do not
        // care about timestamps, as we trust the syslog takes
        // care of that.  We also do not do anything about
        // colours, as that can mess up the log files.

        if (!metadata.empty())
        {
            syslog(LOG_INFO, "%s", metadata.c_str());
            metadata.clear();
        }

        syslog(LOG_INFO, "%s", data.c_str());
    }


    virtual void Write(const LogGroup grp, const LogCategory ctg,
                       const std::string& data,
                       const std::string& colour_init,
                       const std::string& colour_reset)
    {
        // Equally simple to the other Write() method, but here
        // we have access to LogGroup and LogCategory, so we
        // include that information.
        if (!metadata.empty())
        {
            syslog(logcatg2syslog(ctg), "%s", metadata.c_str());
            metadata.clear();
        }
        syslog(logcatg2syslog(ctg), "%s%s",
               LogPrefix(grp, ctg).c_str(), data.c_str());
    }


private:
    /**
     *  Simple conversion between LogCategory and a corresponding
     *  log level used by syslog(3).
     *
     * @param catg  LogCategory to convert to syslog log level.
     *
     * @return  Returns an int of the correpsonding syslog log level value.
     */
    static inline int logcatg2syslog(LogCategory catg)
    {
        switch(catg)
        {
        case LogCategory::DEBUG:
            return LOG_DEBUG;

        case LogCategory::WARN:
            return LOG_WARNING;

        case LogCategory::ERROR:
            return LOG_ERR;

        case LogCategory::CRIT:
            return LOG_CRIT;

        case LogCategory::FATAL:
            return LOG_ALERT;

        case LogCategory::VERB2:
        case LogCategory::VERB1:
        case LogCategory::INFO:
        case LogCategory::UNDEFINED:
        default:
            return LOG_INFO;
        }
    }
};
