//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   syslog.hpp
 *
 * @brief  Declaration of the SyslogWriter implementation of LogWriter
 */

#pragma once

#include <syslog.h>
#include <string.h>

#include "log/logwriter.hpp"
#include "log/log-helpers.hpp"


class SyslogException : public std::exception
{
  public:
    SyslogException(const std::string &err)
        : err(err)
    {
    }

    virtual const char *what() const noexcept
    {
        return err.c_str();
    }

  private:
    std::string err;
};



class SyslogWriter : public LogWriter
{
  public:
    /**
     *  Initialize the SyslogWriter
     *
     * @param prgname       std::string containing the program identifier in the
     *                      syslog calls
     * @param log_facility  Syslog facility to use for log messages.
     *                      (Default: LOG_DAEMON)
     */
    SyslogWriter(const std::string &prgname = NULL,
                 const int log_facility = LOG_DAEMON);
    virtual ~SyslogWriter();

    const std::string GetLogWriterInfo() const override;

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
    bool TimestampEnabled() override;

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
    static inline int ConvertLogFacility(const std::string &facility)
    {
        struct log_facility_mapping_t
        {
            const std::string name;
            const int facility;
        };

        static const struct log_facility_mapping_t log_facilities[] = {
            {"LOG_AUTH", LOG_AUTH},
            {"LOG_AUTHPRIV", LOG_AUTHPRIV},
            {"LOG_CRON", LOG_CRON},
            {"LOG_DAEMON", LOG_DAEMON},
            {"LOG_FTP", LOG_FTP},
            {"LOG_KERN", LOG_KERN},
            {"LOG_LOCAL0", LOG_LOCAL0},
            {"LOG_LOCAL1", LOG_LOCAL1},
            {"LOG_LOCAL2", LOG_LOCAL2},
            {"LOG_LOCAL3", LOG_LOCAL3},
            {"LOG_LOCAL4", LOG_LOCAL4},
            {"LOG_LOCAL5", LOG_LOCAL5},
            {"LOG_LOCAL6", LOG_LOCAL6},
            {"LOG_LOCAL7", LOG_LOCAL7},
            {"LOG_LPR", LOG_LPR},
            {"LOG_MAIL", LOG_MAIL},
            {"LOG_NEWS", LOG_NEWS},
            {"LOG_SYSLOG", LOG_SYSLOG},
            {"LOG_USER", LOG_USER},
            {"LOG_UUCP", LOG_UUCP}};

        for (auto const &m : log_facilities)
        {
            if (facility == m.name)
            {
                return m.facility;
            }
        }
        throw SyslogException("Invalid syslog facility value: " + facility);
    }


  protected:
    void WriteLogLine(LogTag::Ptr logtag,
                      const std::string &data,
                      const std::string &colour_init = "",
                      const std::string &colour_reset = "") override;


    void WriteLogLine(LogTag::Ptr logtag,
                      const LogGroup grp,
                      const LogCategory ctg,
                      const std::string &data,
                      const std::string &colour_init,
                      const std::string &colour_reset) override;


  private:
    char *progname = nullptr;

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
        switch (catg)
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
