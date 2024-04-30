//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   dbus-log.cpp
 *
 * @brief  Implementation of the OpenVPN 3 Linux D-Bus logging based interface
 */

#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>
#include <gdbuspp/signals/target.hpp>

#include "dbus-log.hpp"


//
//  LogFilter class implementation
//

LogFilter::LogFilter(const uint32_t loglvl) noexcept
    : log_level(loglvl)
{
}


void LogFilter::SetLogLevel(const uint32_t loglev)
{
    if (loglev > 6)
    {
        throw LogException("LogSender: Invalid log level");
    }
    log_level = loglev;
}


uint32_t LogFilter::GetLogLevel() noexcept
{
    return log_level;
}


void LogFilter::AddPathFilter(const std::string &path)
{
    filter_paths.push_back(path);
    std::sort(filter_paths.begin(), filter_paths.end());
}


bool LogFilter::LogFilterAllow(const Events::Log &logev) noexcept
{
    switch (logev.category)
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


bool LogFilter::AllowPath(const std::string &path) noexcept
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

LogSender::LogSender(DBus::Connection::Ptr dbuscon,
                     const LogGroup lgroup,
                     const std::string &objpath,
                     const std::string &interf,
                     const bool session_token,
                     LogWriter *lgwr,
                     const bool disable_stathschg)
    : DBus::Signals::Group(dbuscon, objpath, interf),
      LogFilter(3),
      logwr(lgwr),
      log_group(lgroup)
{
    RegisterSignal("Log",
                   Events::Log::SignalDeclaration(session_token));

    if (!disable_stathschg)
    {
        RegisterSignal("StatusChange",
                       Events::Status::SignalDeclaration());
    }
}


const LogGroup LogSender::GetLogGroup() const
{
    return log_group;
}


void LogSender::StatusChange(const Events::Status &statusev)
{
    SendGVariant("StatusChange", statusev.GetGVariantTuple());
}


void LogSender::ProxyLog(const Events::Log &logev, const std::string &path)
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
        SendGVariant("Log", logev.GetGVariantTuple());
    }
}


void LogSender::ProxyStatusChange(const Events::Status &status, const std::string &path)
{
    if (!status.empty() && AllowPath(path))
    {
        StatusChange(status);
    }
}


void LogSender::Log(const Events::Log &logev, const bool duplicate_check, const std::string &target)
{
    // Don't log an empty tmessages or if log level filtering allows it
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

    if (logwr)
    {
        logwr->Write(logev);
    }

    SendGVariant("Log", logev.GetGVariantTuple());
}


void LogSender::Debug(const std::string &msg, const bool duplicate_check)
{
    Log(Events::Log(log_group, LogCategory::DEBUG, msg), duplicate_check);
}


void LogSender::LogVerb2(const std::string &msg, const bool duplicate_check)
{
    Log(Events::Log(log_group, LogCategory::VERB2, msg), duplicate_check);
}


void LogSender::LogVerb1(const std::string &msg, const bool duplicate_check)
{
    Log(Events::Log(log_group, LogCategory::VERB1, msg), duplicate_check);
}


void LogSender::LogInfo(const std::string &msg, const bool duplicate_check)
{
    Log(Events::Log(log_group, LogCategory::INFO, msg), duplicate_check);
}


void LogSender::LogWarn(const std::string &msg, const bool duplicate_check)
{
    Log(Events::Log(log_group, LogCategory::WARN, msg), duplicate_check);
}


void LogSender::LogError(const std::string &msg)
{
    Log(Events::Log(log_group, LogCategory::ERROR, msg));
}


void LogSender::LogCritical(const std::string &msg)
{
    // Critical log messages will always be sent
    Log(Events::Log(log_group, LogCategory::CRIT, msg));
}


void LogSender::LogFATAL(const std::string &msg)
{
    // Fatal log messages will always be sent
    Log(Events::Log(log_group, LogCategory::FATAL, msg));
    // FIXME: throw something here, to start shutdown procedures
}


Events::Log LogSender::GetLastLogEvent() const
{
    return Events::Log(last_logevent);
}


LogWriter *LogSender::GetLogWriter()
{
    return logwr;
}



//
//  LogConsumer class implementation
//
LogConsumer::LogConsumer(DBus::Connection::Ptr dbuscon,
                         const std::string &interf,
                         const std::string &objpath,
                         const std::string &busn)
    : LogFilter(6) // By design, accept all kinds of log messages when receiving
{
    subscriptions = DBus::Signals::SubscriptionManager::Create(dbuscon);
    subscription_target = DBus::Signals::Target::Create(busn, objpath, interf);
    subscriptions->Subscribe(subscription_target,
                             "Log",
                             [this](DBus::Signals::Event::Ptr event)
                             {
                                 this->process_log_event(event);
                             });
}


//
//  LogConsumerProxyException implementation
//

LogConsumerProxyException::LogConsumerProxyException(LogProxyExceptionType t) noexcept
    : type(t), message()
{
}


LogConsumerProxyException::LogConsumerProxyException(LogProxyExceptionType t,
                                                     const std::string &message_str) noexcept
    : type(t), message(std::move(message_str))
{
}


const char *LogConsumerProxyException::what() const noexcept
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

LogConsumerProxy::LogConsumerProxy(DBus::Connection::Ptr dbuscon,
                                   const std::string &src_interf,
                                   const std::string &src_objpath,
                                   const std::string &dst_interf,
                                   const std::string &dst_objpath)
    : LogConsumer(dbuscon, src_interf, src_objpath),
      LogSender(dbuscon, LogGroup::UNDEFINED, dst_interf, dst_objpath)
{
}


void LogConsumerProxy::process_log_event(DBus::Signals::Event::Ptr event)
{
    try
    {
        Events::Log logev(event->params);
        Events::Log ev = InterceptLogEvent(event->sender,
                                           event->object_interface,
                                           event->object_path,
                                           logev);
        ProxyLog(ev);
    }
    catch (const LogConsumerProxyException &excp)
    {
        switch (excp.GetExceptionType())
        {
        case LogProxyExceptionType::IGNORE:
            // The InterceptLogEvent method asks to ignore it and not
            // proxy this log event, so we return
            return;

        case LogProxyExceptionType::INVALID:
            // Re-throw the exception if the log event is invalid
            throw LogException("LogConsumerProxy: "
                               + std::string(excp.what()));

        default:
            throw LogException("LogConsmerProxy:  Unknown error");
        }
    }
    catch (...)
    {
        throw;
    }
}
