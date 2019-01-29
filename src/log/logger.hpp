//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2018 OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017-2018 David Sommerseth <davids@openvpn.net>
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

#pragma once

/**
 * @file   logger.hpp
 *
 * @brief  Main log handler class, handles all the Log signals being sent
 */

#include "dbus-log.hpp"
#include "logwriter.hpp"


class Logger : public LogConsumer,
               public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<Logger> Ptr;

    Logger(GDBusConnection *dbuscon, LogWriter *logwr,
           const std::string& tag, const std::string& busname,
           const std::string& interf,
           const unsigned int log_level = 3)
        : LogConsumer(dbuscon, interf, "", busname),
          logwr(logwr),
          log_tag(tag)
    {
        SetLogLevel(log_level);
    }


    /**
     *  Adds a LogGroup to be excluded when consuming log events
     * @param exclgrp LogGroup to exclude
     */
    void AddExcludeFilter(LogGroup exclgrp)
    {
        exclude_loggroup.push_back(exclgrp);
    }


    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogEvent& logev)
    {
        for (const auto& e : exclude_loggroup)
        {
            if (e == logev.group)
            {
                return;  // Don't do anything if this LogGroup is filtered
            }
        }

        // Prepend log lines with the log tag
        logwr->WritePrepend(log_tag + std::string(" "), true);

        // Add the meta information
        std::stringstream meta;
        meta << "sender=" << sender
             << ", interface=" << interface
             << ", path=" << object_path;
        if (!logev.session_token.empty())
        {
            meta << ", session-token=" << logev.session_token;
        }
        logwr->AddMeta(meta.str());

        // And write the real log line
        logwr->Write(logev);
    }


private:
    LogWriter *logwr;
    const std::string log_tag;
    std::vector<LogGroup> exclude_loggroup;
};
