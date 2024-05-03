//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#pragma once

#include <ctime>
#include <exception>
#include <fstream>
#include <memory>
#include <string>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "events/status.hpp"
#include "logfilter.hpp"
#include "logwriter.hpp"


class LogSender : public DBus::Signals::Group,
                  public Log::EventFilter
{
  public:
    using Ptr = std::shared_ptr<LogSender>;

    LogSender(DBus::Connection::Ptr dbuscon,
              const LogGroup lgroup,
              const std::string &objpath,
              const std::string &interf,
              const bool session_token = false,
              LogWriter *lgwr = nullptr,
              const bool disable_stathschg = false);
    virtual ~LogSender() = default;

    const LogGroup GetLogGroup() const;

    virtual void StatusChange(const Events::Status &statusev);

    void ProxyLog(const Events::Log &logev, const std::string &path = "");
    void ProxyStatusChange(const Events::Status &status, const std::string &path);

    virtual void Log(const Events::Log &logev, const bool duplicate_check = false, const std::string &target = "");
    virtual void Debug(const std::string &msg, const bool duplicate_check = false);
    virtual void LogVerb2(const std::string &msg, const bool duplicate_check = false);
    virtual void LogVerb1(const std::string &msg, const bool duplicate_check = false);
    virtual void LogInfo(const std::string &msg, const bool duplicate_check = false);
    virtual void LogWarn(const std::string &msg, const bool duplicate_check = false);
    virtual void LogError(const std::string &msg);
    virtual void LogCritical(const std::string &msg);
    virtual void LogFATAL(const std::string &msg);
    Events::Log GetLastLogEvent() const;

    LogWriter *GetLogWriter();


  protected:
    LogWriter *logwr = nullptr;
    LogGroup log_group;


  private:
    Events::Log last_logevent;
};



class LogConsumer : public Log::EventFilter
{
  public:
    LogConsumer(DBus::Connection::Ptr dbusconn,
                const std::string &interf,
                const std::string &objpath,
                const std::string &busn = "");
    virtual ~LogConsumer() = default;


    // clang-format off
    // bug in clang-format causes "= 0" to be wrapped
    virtual void ConsumeLogEvent(const std::string &sender,
                                 const std::string &interface_name,
                                 const std::string &obj_path,
                                 const Events::Log &logev) = 0;
    // clang-format on

  protected:
    /**
     *  Signals received with the signal name "Log" are processed by
     *  this method.
     *
     *  Default implementation passes the LogEvent to the ConsumeLogEvent()
     *  method if the log_level is high enough for the LogEvent (ie. not
     *  filtered out)
     *
     */
    virtual void process_log_event(DBus::Signals::Event::Ptr event)
    {
        // Pass the signal parameters to be parsed by LogEvent directly
        Events::Log logev(event->params);

        if (!EventFilter::Allow(logev))
        {
            return;
        }
        ConsumeLogEvent(event->sender,
                        event->object_interface,
                        event->object_path,
                        logev);
    }

  protected:
    DBus::Signals::SubscriptionManager::Ptr subscriptions = nullptr;
    DBus::Signals::Target::Ptr subscription_target = nullptr;
};



enum class LogProxyExceptionType : std::uint8_t
{
    IGNORE,
    INVALID
};


class LogConsumerProxyException : public std::exception
{
  public:
    LogConsumerProxyException(LogProxyExceptionType t) noexcept;
    LogConsumerProxyException(LogProxyExceptionType t,
                              const std::string &message_str) noexcept;
    ~LogConsumerProxyException() = default;

    const char *what() const noexcept;
    LogProxyExceptionType GetExceptionType() const noexcept;


  private:
    LogProxyExceptionType type;
    std::string message;
};



class LogConsumerProxy : public LogConsumer, public LogSender
{
  public:
    LogConsumerProxy(DBus::Connection::Ptr dbusconn,
                     const std::string &src_interf,
                     const std::string &src_objpath,
                     const std::string &dst_interf,
                     const std::string &dst_objpath);
    ~LogConsumerProxy() = default;


    void ConsumeLogEvent(const std::string &sender,
                         const std::string &interface,
                         const std::string &object_path,
                         const Events::Log &logev)
    {
        // This is a dummy method and is not used by LogConsumerProxy.
        // The InterceptLogEvent() method is used instead, which allows
        // the LogEvent to be modified on-the-fly _before_ being proxied.
    }


    // clang-format off
    // bug in clang-format causes "= 0" to be wrapped
    virtual Events::Log InterceptLogEvent(const std::string &sender,
                                       const std::string &interface,
                                       const std::string &object_path,
                                       const Events::Log &logev) = 0;
    // clang-format on


  protected:
    virtual void process_log_event(DBus::Signals::Event::Ptr event);
};
