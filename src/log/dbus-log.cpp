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
//  LogSender class implementation
//

LogSender::LogSender(DBus::Connection::Ptr dbuscon,
                     const LogGroup lgroup,
                     const std::string &objpath,
                     const std::string &interf,
                     const bool session_token,
                     LogWriter *lgwr)
    : DBus::Signals::Group(dbuscon, objpath, interf),
      Log::EventFilter(3),
      logwr(lgwr),
      log_group(lgroup)
{
    RegisterSignal("Log",
                   Events::Log::SignalDeclaration(session_token));
}


const LogGroup LogSender::GetLogGroup() const
{
    return log_group;
}


void LogSender::Log(const Events::Log &logev, const bool duplicate_check, const std::string &target)
{
    // Don't log an empty messages or if log level filtering allows it
    // The filtering is done against the LogCategory of the message
    if (logev.empty() || !EventFilter::Allow(logev))
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
    : Log::EventFilter(6) // By design, accept all kinds of log messages when receiving
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
