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
 * @file   logwriter.hpp
 *
 * @brief  Base class implementing a generic logging interface (API)
 *         as well as file stream and syslog implementations.
 */

#pragma once

#include <syslog.h>

#include <algorithm>
#include <fstream>
#include <exception>
#include <string>
#include <vector>

#include "common/timestamp.hpp"
#include "colourengine.hpp"
#include "logevent.hpp"
#include "logtag.hpp"


struct LogMetaDataValue
{
    enum class Type {
        LOGMETA_STRING,
        LOGMETA_LOGTAG
    };

    using Ptr = std::shared_ptr<LogMetaDataValue>;

    LogMetaDataValue(const std::string& l, const std::string& v, bool s = false)
        : label(l), str_value(v), logtag((const LogTag&)LogTag()), skip(s)
    {
        type = Type::LOGMETA_STRING;
    }

    LogMetaDataValue(const std::string& l, const LogTag& v, bool s = false)
        : label(l), str_value(""), logtag(v), skip(s)
    {
        type = Type::LOGMETA_LOGTAG;
    }

    template<typename T>
    static LogMetaDataValue::Ptr create(const std::string& l,
                                        const T& v,
                                        bool s = false)
    {
        LogMetaDataValue::Ptr ret;
        ret.reset(new LogMetaDataValue(l, v, s));
        return ret;
    }


    const std::string GetValue(const bool logtag_encaps=true) const
    {
        switch(type)
        {
        case Type::LOGMETA_STRING:
            return str_value;

        case Type::LOGMETA_LOGTAG:
            return logtag.str(logtag_encaps);
        }
        return "";
    }


    friend std::ostream& operator<<(std::ostream& os, const LogMetaDataValue mdv)
    {
        if (mdv.skip)
        {
            return os;
        }
        return os << mdv.label << "=" << mdv.GetValue(mdv.logtag.encaps);
    }


    Type type;
    std::string label;
    const std::string str_value;
    const LogTag& logtag;
    bool skip;
};


class LogMetaData
{
public:
    using Ptr = std::shared_ptr<LogMetaData>;
    using Records = std::vector<std::string>;

    LogMetaData() {};
    ~LogMetaData() = default;


    static LogMetaData::Ptr create()
    {
        LogMetaData::Ptr ret;
        ret.reset(new LogMetaData);
        return ret;
    }


    template<typename T>
    void AddMeta(const std::string& l, const T& v, bool skip = false)
    {
        auto mdv = LogMetaDataValue::create(l, v, skip);
        metadata.push_back(mdv);
    }


    std::string GetMetaValue(const std::string l, const bool encaps_logtag=true,
                             const std::string postfix=" ") const
    {
        auto it = std::find_if(metadata.begin(), metadata.end(),
                               [l](LogMetaDataValue::Ptr e)
                               {
                                   return l == e->label;
                               });
        if (metadata.end() == it)
        {
            return "";
        }
        return (*it)->GetValue(encaps_logtag) + postfix;
    }


    Records GetMetaDataRecords(const bool upcase_label=false,
                               const bool logtag_encaps=true) const
    {
        Records ret;
        for (const auto& mdc : metadata)
        {
            std::string label = mdc->label;
            if (upcase_label)
            {
                std::transform(label.begin(), label.end(), label.begin(),
                               [](unsigned char c){ return std::toupper(c); });
            }
            if (LogMetaDataValue::Type::LOGMETA_LOGTAG == mdc->type)
            {
                ret.push_back(label + "=" + mdc->GetValue(logtag_encaps));
            }
            else
            {
                ret.push_back(label + "=" + mdc->GetValue());
            }
        }
        return ret;
    }


    size_t size() const
    {
        return metadata.size();
    }


    bool empty() const
    {
        return metadata.empty();
    }

    void clear()
    {
        metadata.clear();
    }


    friend std::ostream& operator<<(std::ostream& os, const LogMetaData& mdc)
    {
        bool first = true;
        for (const auto& mdv : mdc.metadata)
        {
            if (mdv->skip)
            {
                continue;
            }
            os << (!first ? ", " : "") << (*mdv);
            if (first)
            {
                first = false;
            }
        }
        return os;
    }

private:
    std::vector<LogMetaDataValue::Ptr> metadata;
};



/**
 *  Base class providing a generic API for writing log data
 *  to an output stream
 */
class LogWriter
{
public:
    typedef std::unique_ptr<LogWriter> Ptr;

    LogWriter() = default;
    virtual ~LogWriter() = default;


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
        log_meta= meta;
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
     * @param logev  Populated LogEvent() object to log
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
    virtual void AddMeta(const std::string& label, const std::string& data,
                         bool skip=false)
    {
        if (log_meta)
        {
            metadata.AddMeta(label, data, skip);
        }
    }


    virtual void AddMeta(const std::string& label, const LogTag& ltg,
                         const bool skip=false, const bool encaps=true)
    {
        if (log_meta)
        {
            metadata.AddMeta(label, ltg, skip);
        }
    }


    void AddMetaCopy(const LogMetaData& mdc)
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
    virtual void PrependMeta(const std::string& label, bool prep_meta = false)
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
     *                     set the proper colours.  Empty by default.
     * @param colour_reset std::string to be printed after the log data
     *                     to reset colour selection.  Empty by default.
     */
    virtual void Write(const std::string& data,
                       const std::string& colour_init = "",
                       const std::string& colour_reset = "") override
    {
        if (log_meta && !metadata.empty())
        {
            dest << (timestamp ? GetTimestamp() : "") << " "
                 << colour_init;
            if (prepend_meta)
            {
                 dest << metadata.GetMetaValue(prepend_label);
            }
            dest << metadata << colour_reset
                 << std::endl;
            prepend_meta = false;
        }
        dest << (timestamp ? GetTimestamp() : "") << " "
             << colour_init;
        if (!prepend_label.empty())
        {
            dest << metadata.GetMetaValue(prepend_label);
        }
        dest << data << colour_reset << std::endl;
        prepend_label.clear();
        metadata.clear();
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

    virtual ~ColourStreamWriter() = default;


    /*
     * Explicitly tells the compiler that we want to not to override an
     * existing Write and our Write with different arguments should not
     * generate a warning
     */
    using StreamLogWriter::Write;

    virtual void Write(const LogGroup grp,
                       const LogCategory ctg,
                       const std::string& data) override
    {
        switch (colours->GetColourMode())
        {
        case ColourEngine::ColourMode::BY_CATEGORY:
            LogWriter::Write(grp, ctg, data,
                             colours->ColourByCategory(ctg),
                             colours->Reset());
            return;

        case ColourEngine::ColourMode::BY_GROUP:
            {
                std::string grpcol = colours->ColourByGroup(grp);
                // Highlights parts of the log event which are higher than LogCategory::INFO
                std::string ctgcol = (LogCategory::INFO < ctg ? colours->ColourByCategory(ctg) : grpcol);
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
};


class SyslogException : public std::exception
{
public:
    SyslogException(const std::string& err)
        : err(err)
    {
    }

    virtual const char* what() const noexcept
    {
        return err.c_str();
    }

private:
    std::string err;
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

    /**
     *  We presume syslog will always add timestamps to its logging,
     *  so we will return true regardless of what an external user wants.
     *
     *  In addition, SyslogWriter doesn't even care about the timestamp
     *  flag.  So try to present a value which is more likely true regardless
     *  of this internal flag.
     *
     * @return Will always return true.
     */
    virtual bool TimestampEnabled() override
    {
        return true;
    }


    /**
     *  Converts a string specifying a syslog log facility
     *  to the appropriate into syslog integer value.  See syslog(3)
     *  for valid values.
     *
     * @param facility  std::string containing the log facility name
     *
     * @return Returns the integer reference to the log facility,
     *         compliant to syslog(3).  In case of an invalid
     */
    static inline int ConvertLogFacility(const std::string& facility)
    {
        struct log_facility_mapping_t {
            const std::string name;
            const int facility;
        };

        static const struct log_facility_mapping_t log_facilities[] =
            {
                {"LOG_AUTH",     LOG_AUTH},
                {"LOG_AUTHPRIV", LOG_AUTHPRIV},
                {"LOG_CRON",     LOG_CRON},
                {"LOG_DAEMON",   LOG_DAEMON},
                {"LOG_FTP",      LOG_FTP},
                {"LOG_KERN",     LOG_KERN},
                {"LOG_LOCAL0",   LOG_LOCAL0},
                {"LOG_LOCAL1",   LOG_LOCAL1},
                {"LOG_LOCAL2",   LOG_LOCAL2},
                {"LOG_LOCAL3",   LOG_LOCAL3},
                {"LOG_LOCAL4",   LOG_LOCAL4},
                {"LOG_LOCAL5",   LOG_LOCAL5},
                {"LOG_LOCAL6",   LOG_LOCAL6},
                {"LOG_LOCAL7",   LOG_LOCAL7},
                {"LOG_LPR",      LOG_LPR},
                {"LOG_MAIL",     LOG_MAIL},
                {"LOG_NEWS",     LOG_NEWS},
                {"LOG_SYSLOG",   LOG_SYSLOG},
                {"LOG_USER",     LOG_USER},
                {"LOG_UUCP",     LOG_UUCP}
            };

        for (auto const& m : log_facilities)
        {
            if (facility == m.name)
            {
                return m.facility;
            }
        }
        throw SyslogException("Invalid syslog facility value: " + facility);
    }


    virtual void Write(const std::string& data,
                       const std::string& colour_init = "",
                       const std::string& colour_reset = "") override
    {
        // This is a very simple log implementation.  We do not
        // care about timestamps, as we trust the syslog takes
        // care of that.  We also do not do anything about
        // colours, as that can mess up the log files.

        std::ostringstream p;
        p << (prepend_meta ? metadata.GetMetaValue(prepend_label) : "");

        if (log_meta && !metadata.empty())
        {
            std::ostringstream m;
            m << metadata;

            syslog(LOG_INFO, "%s%s", p.str().c_str(), m.str().c_str());
            prepend_meta = false;
        }

        syslog(LOG_INFO, "%s%s", p.str().c_str(), data.c_str());
        prepend_label.clear();
        metadata.clear();
    }


    virtual void Write(const LogGroup grp, const LogCategory ctg,
                       const std::string& data,
                       const std::string& colour_init,
                       const std::string& colour_reset) override
    {
        // Equally simple to the other Write() method, but here
        // we have access to LogGroup and LogCategory, so we
        // include that information.
        std::ostringstream p;
        p << (prepend_meta ? metadata.GetMetaValue(prepend_label) : "");

        if (log_meta && !metadata.empty())
        {
            std::ostringstream m;
            m << metadata;

            syslog(logcatg2syslog(ctg), "%s%s",
                   p.str().c_str(), m.str().c_str());
            prepend_meta = false;
        }

        syslog(logcatg2syslog(ctg), "%s%s%s",
               p.str().c_str(), LogPrefix(grp, ctg).c_str(), data.c_str());
        prepend_label.clear();
        metadata.clear();
    }


private:
    /**
     *  Simple conversion between LogCategory and a corresponding
     *  log level used by syslog(3).
     *
     * @param catg  LogCategory to convert to syslog log level.
     *
     * @return  Returns an int of the corresponding syslog log level value.
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
