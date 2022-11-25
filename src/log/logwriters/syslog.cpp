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
    p << (prepend_meta ? metadata.GetMetaValue(prepend_label) : "");

    if (log_meta && !metadata.empty())
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
    metadata.clear();
}
