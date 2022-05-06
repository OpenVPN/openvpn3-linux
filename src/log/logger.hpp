//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017-2022 OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017-2022 David Sommerseth <davids@openvpn.net>
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

#include <memory>

#include "dbus-log.hpp"
#include "logwriter.hpp"


class Logger : public LogConsumer
{
public:
    typedef std::shared_ptr<Logger> Ptr;

    Logger(GDBusConnection *dbuscon, LogWriter *logwr,
           const std::string& tag, const std::string& busname,
           const std::string& interf,
           const unsigned int log_level = 3)
        : LogConsumer(dbuscon, interf, "", busname),
          logwr(logwr),
          log_tag(tag)
    {
        SetLogLevel(log_level);
        Subscribe("StatusChange");
    }


    /**
     *  Adds a LogGroup to be excluded when consuming log events
     * @param exclgrp LogGroup to exclude
     */
    void AddExcludeFilter(LogGroup exclgrp)
    {
        exclude_loggroup.push_back(exclgrp);
    }


    /**
     *  Adds a log forwarding possibility through an additional LogSender
     *  based object.  More LogSender objects can be registered, but the
     *  provided logid needs to be unique.
     *
     * @param prx     A raw pointer to the LogSender object
     * @param logid   std::string with a unique ID to this log forwarder
     */
    void AddLogForward(LogSender* prx, const std::string& logid)
    {
        if (logid.empty())
        {
            THROW_DBUSEXCEPTION("Logger", "No LogID set in LogSender object");
        }
        log_forwards[logid] = prx;
        logwr->Write(LogGroup::LOGGER, LogCategory::DEBUG,
                     "[Logger] Log forward added for " + logid);
    }


    /**
     *  Remove a a single LogSender forwarding.
     *
     * @param logid  std::string of the ID to the log forwarder to remove
     */
    void RemoveLogForward(const std::string logid)
    {
        log_forwards[logid] = nullptr;
        log_forwards.erase(logid);
        logwr->Write(LogGroup::LOGGER, LogCategory::DEBUG,
                     "[Logger] Log forward removed for " + logid);
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

        // If there are any log forwarders attached, do the forwarding
        if (log_forwards.size() > 0)
        {
            LogEvent proxy_event(logev);
            proxy_event.RemoveToken();
            for (const auto& lfwd : log_forwards)
            {
                if (lfwd.second)
                {
                    lfwd.second->ProxyLog(proxy_event, object_path);
                }
            }
        }
    }


    void ProcessSignal(const std::string sender_name,
                       const std::string obj_path,
                       const std::string interface_name,
                       const std::string signal_name,
                       GVariant *parameters) override
    {
        if ("StatusChange" != signal_name)
        {
            // Fail-safe: Only care about StatusChange signals
            return;
        }
        for (const auto& lfwd : log_forwards)
        {
            StatusEvent status(parameters);
            lfwd.second->ProxyStatusChange(status, obj_path);
        }
    }


private:
    LogWriter *logwr;
    const std::string log_tag;
    std::vector<LogGroup> exclude_loggroup;
    std::map<std::string, LogSender*> log_forwards = {};
};
