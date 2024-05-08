//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   syslog.cpp
 *
 * @brief  Implementation of SyslogWriter
 */

#include <iostream>
#include <sstream>
#include <string>

#include "../logwriter.hpp"
#include "syslog.hpp"


//
//  SyslogWriter - implementation
//
SyslogWriter::SyslogWriter(const std::string &prgname,
                           const int log_facility)
    : LogWriter()
{
    progname = strdup(prgname.c_str());
    openlog(progname, LOG_NDELAY | LOG_PID, log_facility);
}

SyslogWriter::~SyslogWriter()
{
    closelog();
    free(progname);
}


const std::string SyslogWriter::GetLogWriterInfo() const
{
    return std::string("syslog");
}


bool SyslogWriter::TimestampEnabled()
{
    return true;
}


void SyslogWriter::Write(const std::string &data,
                         const std::string &colour_init,
                         const std::string &colour_reset)
{
    // This is a very simple log implementation.  We do not
    // care about timestamps, as we trust the syslog takes
    // care of that.  We also do not do anything about
    // colours, as that can mess up the log files.

    std::ostringstream p;
    p << (prepend_meta ? metadata->GetMetaValue(prepend_label) : "");

    if (log_meta && !metadata->empty())
    {
        std::ostringstream m;
        m << metadata;

        syslog(LOG_INFO, "%s%s", p.str().c_str(), m.str().c_str());
        prepend_meta = false;
    }

    syslog(LOG_INFO, "%s%s", p.str().c_str(), data.c_str());
    prepend_label.clear();
    metadata->clear();
}


void SyslogWriter::Write(const LogGroup grp,
                         const LogCategory ctg,
                         const std::string &data,
                         const std::string &colour_init,
                         const std::string &colour_reset)
{
    // Equally simple to the other Write() method, but here
    // we have access to LogGroup and LogCategory, so we
    // include that information.
    std::ostringstream p;
    p << (prepend_meta ? metadata->GetMetaValue(prepend_label) : "");

    if (log_meta && !metadata->empty())
    {
        std::ostringstream m;
        m << metadata;

        syslog(logcatg2syslog(ctg),
               "%s%s",
               p.str().c_str(),
               m.str().c_str());
        prepend_meta = false;
    }

    syslog(logcatg2syslog(ctg),
           "%s%s%s",
           p.str().c_str(),
           LogPrefix(grp, ctg).c_str(),
           data.c_str());
    prepend_label.clear();
    metadata->clear();
}
