//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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
 * @file   dbus-log.cpp
 *
 * @brief  Implementation of the OpenVPN 3 Linux D-Bus logging based interface
 */

#include "dbus-log.hpp"


//
//  LogFilter class implementation
//

LogFilter::LogFilter(unsigned int loglvl) noexcept
        : log_level(loglvl)
{
}


void LogFilter::SetLogLevel(unsigned int loglev)
{
    if (loglev > 6)
    {
        THROW_LOGEXCEPTION("LogSender: Invalid log level");
    }
    log_level = loglev;
}


unsigned int LogFilter::GetLogLevel() noexcept
{
    return log_level;
}


void LogFilter::AddPathFilter(const std::string& path)
{
    filter_paths.push_back(path);
    std::sort(filter_paths.begin(), filter_paths.end());
}


bool LogFilter::LogFilterAllow(const LogEvent& logev) noexcept
{
    switch(logev.category)
    {
    case LogCategory::DEBUG:
        return log_level >= 6;
    case LogCategory::VERB2:
        return log_level >= 5;
    case LogCategory::VERB1:
        return log_level >= 4;
    case LogCategory::INFO:
        return log_level >= 3;
    case LogCategory::WARN:
        return log_level >= 2;
    case LogCategory::ERROR:
        return log_level >= 1;
    default:
        return true;
    }
}


bool LogFilter::AllowPath(const std::string& path) noexcept
{
    if (filter_paths.size() < 1)
    {
        return true;
    }
    auto r = std::lower_bound(filter_paths.begin(), filter_paths.end(), path);
    return r != filter_paths.end();
}


//
//  LogSender class implementation
//

LogSender::LogSender(GDBusConnection *dbuscon, const LogGroup lgroup,
                     std::string interf,
                     std::string objpath,
                     LogWriter *lgwr)
        : DBusSignalProducer(dbuscon, "", interf, objpath),
          LogFilter(3),
          logwr(lgwr),
          log_group(lgroup)
{
}


const std::string LogSender::GetLogIntrospection()
{
    return
        "        <signal name='Log'>"
        "            <arg type='u' name='group' direction='out'/>"
        "            <arg type='u' name='level' direction='out'/>"
        "            <arg type='s' name='message' direction='out'/>"
        "        </signal>";
}


const std::string LogSender::GetStatusChangeIntrospection()
{
    return
        "        <signal name='StatusChange'>"
        "            <arg type='u' name='code_major' direction='out'/>"
        "            <arg type='u' name='code_minor' direction='out'/>"
        "            <arg type='s' name='message' direction='out'/>"
        "        </signal>";
}


void LogSender::StatusChange(const StatusEvent& statusev)
{
    Send("StatusChange", statusev.GetGVariantTuple());
}


void LogSender::ProxyLog(const LogEvent& logev, const std::string& path)
{
    // Don't proxy an empty log message and if the log level filtering
    // allows it.  The filtering is done against the LogCategory of
    // the message, so we need to extract the LogCategory first
    if (!logev.empty() && LogFilterAllow(logev))
    {
        // If a path check is added, only proxy log event if path is allowed
        if (path.length() > 0 && !AllowPath(path))
        {
            return;
        }
        Send("Log", logev.GetGVariantTuple());
    }
}


void LogSender::ProxyStatusChange(const StatusEvent& status, const std::string& path)
{
    if (!status.empty() && AllowPath(path))
    {
        StatusChange(status);
    }
}


void LogSender::Log(const LogEvent& logev, bool duplicate_check,
                    const std::string& target)
{
    // Don't log an empty messages or if log level filtering allows it
    // The filtering is done against the LogCategory of the message
    if (logev.empty() || !LogFilterAllow(logev))
    {
        return;
    }

    if (duplicate_check)
    {
        if (!last_logevent.empty() && (logev == last_logevent))
        {
            // This contains the same log message as the previous one
            return;
        }
    }
    last_logevent = logev;

    if( logwr )
    {
        logwr->Write(logev);
    }

    SendTarget(target, "Log", logev.GetGVariantTuple());
}


void LogSender::Debug(std::string msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::DEBUG, msg), duplicate_check);
}


void LogSender::LogVerb2(std::string msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::VERB2, msg), duplicate_check);
}


void LogSender::LogVerb1(std::string msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::VERB1, msg), duplicate_check);
}


void LogSender::LogInfo(std::string msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::INFO, msg), duplicate_check);
}


void LogSender::LogWarn(std::string msg, bool duplicate_check)
{
    Log(LogEvent(log_group, LogCategory::WARN, msg), duplicate_check);
}


void LogSender::LogError(std::string msg)
{
    Log(LogEvent(log_group, LogCategory::ERROR, msg));
}


void LogSender::LogCritical(std::string msg)
{
    // Critical log messages will always be sent
    Log(LogEvent(log_group, LogCategory::CRIT, msg));
}


void LogSender::LogFATAL(std::string msg)
{
    // Fatal log messages will always be sent
    Log(LogEvent(log_group, LogCategory::FATAL, msg));
    // FIXME: throw something here, to start shutdown procedures
}


LogEvent LogSender::GetLastLogEvent() const
{
    return LogEvent(last_logevent);
}


LogWriter * LogSender::GetLogWriter()
{
    return logwr;
}



//
//  LogConsumer class implementation
//
LogConsumer::LogConsumer(GDBusConnection *dbuscon,
                         std::string interf,
                         std::string objpath,
                         std::string busn)
        : DBusSignalSubscription(dbuscon, busn, interf, objpath, "Log"),
          LogFilter(6) // By design, accept all kinds of log messages when receiving
{
}


void LogConsumer::callback_signal_handler(GDBusConnection *connection,
                                          const std::string sender_name,
                                          const std::string obj_path,
                                          const std::string interface_name,
                                          const std::string signal_name,
                                          GVariant *parameters)
{
    if ("Log" == signal_name)
    {
        process_log_event(sender_name, interface_name, obj_path,
                          parameters);
    }
    else
    {
        ProcessSignal(sender_name, obj_path, interface_name,
                      signal_name, parameters);
    }
}



//
//  LogConsumerProxyException implementation
//

LogConsumerProxyException::LogConsumerProxyException(LogProxyExceptionType t) noexcept
    : type(t), message()
{
}


LogConsumerProxyException::LogConsumerProxyException(LogProxyExceptionType t,
                          const std::string& message_str) noexcept
    : type(t), message(std::move(message_str))
{
}


const char* LogConsumerProxyException::what() const noexcept
{
    return message.c_str();
}


LogProxyExceptionType LogConsumerProxyException::GetExceptionType() const noexcept
{
    return type;
}



//
//  LogConsumerProxy class implementation
//

LogConsumerProxy::LogConsumerProxy(GDBusConnection *dbuscon,
                 std::string src_interf, std::string src_objpath,
                 std::string dst_interf, std::string dst_objpath)
    : LogConsumer(dbuscon, src_interf, src_objpath),
      LogSender(dbuscon, LogGroup::UNDEFINED, dst_interf, dst_objpath)
{
}


void LogConsumerProxy::process_log_event(const std::string sender,
                                         const std::string interface,
                                         const std::string object_path,
                                         GVariant *params)
{
    try
    {
        LogEvent logev(params);
        LogEvent ev = InterceptLogEvent(sender, interface,
                                        object_path, logev);
        ProxyLog(ev);
    }
    catch (const LogConsumerProxyException& excp)
    {
        switch (excp.GetExceptionType())
        {
        case LogProxyExceptionType::IGNORE:
            // The InterceptLogEvent method asks to ignore it and not
            // proxy this log event, so we return
            return;

        case LogProxyExceptionType::INVALID:
            // Re-throw the exception if the log event is invalid
            THROW_LOGEXCEPTION("LogConsumerProxy: "
                               + std::string(excp.what()));

        default:
            THROW_LOGEXCEPTION("LogConsmerProxy:  Unknown error");
        }
    }
    catch(...)
    {
        throw;
    }
}
