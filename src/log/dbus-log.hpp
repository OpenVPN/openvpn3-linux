//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

#pragma once

#include <fstream>
#include <ctime>
#include <exception>
#include <string>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/signals/group.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "client/statusevent.hpp"
#include "log-helpers.hpp"
#include "logevent.hpp"
#include "logwriter.hpp"


/**
 *  Helper class to LogConsumer and LogSender which implements
 *  filtering of log messages.
 */
class LogFilter
{
  public:
    /**
     *  Prepares the log filter
     *
     * @param log_level unsigned int of the default log level
     */
    LogFilter(const unsigned int log_level_val) noexcept;
    virtual ~LogFilter() = default;


    /**
     *  Sets the log level.  This filters which log messages will
     *  be processed or not.  Valid values are 0-6.
     *
     *  Log level 0 - Only FATAL and Critical messages are logged
     *  Log level 1 - includes log level 0 + Error messages
     *  Log level 2 - includes log level 1 + Warning messages
     *  Log level 3 - includes log level 2 + informational messages
     *  Log level 4 - includes log level 3 + Verb 1 messages
     *  Log level 5 - includes log level 4 + Verb 2 messages
     *  Log level 6 - includes log level 5 + Debug messages (everything)
     *
     * @param loglev  unsigned int with the log level to use
     */
    void SetLogLevel(const unsigned int loglev);


    /**
     * Retrieves the log level in use
     *
     * @return unsigned int, with values between 0-6
     *
     */
    unsigned int GetLogLevel() noexcept;


    /**
     *  Adds an allowed path to the path filtering.  This is used when
     *  proxying log and status events, to filter what is being forwarded.
     *
     * @param path  std::string containing the D-Bus object path to add
     */
    void AddPathFilter(const std::string &path);


  protected:
    /**
     * Checks if the LogCategory matches a log level where
     * logging should happen
     *
     * @param logev  LogEvent where to extract the log category from
     *               for the filter
     *
     * @return  Returns true if this LogEvent should be logged, based
     *          on the log category in the LogEvent object
     */
    bool LogFilterAllow(const LogEvent &logev) noexcept;


    /**
     *  Checks if the path is on the path list configured via @AddPathFilter.
     *
     * @param path  std::string of path to check
     * @return Returns true if the path is allowed. If no paths has been added,
     *         it will always return true.
     */
    bool AllowPath(const std::string &path) noexcept;


  private:
    unsigned int log_level;
    std::vector<std::string> filter_paths;
};



class LogSender : public DBus::Signals::Group,
                  public LogFilter
{
  public:
    LogSender(DBus::Connection::Ptr dbuscon,
              const LogGroup lgroup,
              const std::string &objpath,
              const std::string &interf,
              const bool session_token = false,
              LogWriter *lgwr = nullptr);
    virtual ~LogSender() = default;

    void StatusChange(const StatusEvent &statusev);

    void ProxyLog(const LogEvent &logev, const std::string &path = "");
    void ProxyStatusChange(const StatusEvent &status, const std::string &path);

    virtual void Log(const LogEvent &logev, const bool duplicate_check = false, const std::string &target = "");
    virtual void Debug(const std::string &msg, const bool duplicate_check = false);
    virtual void LogVerb2(const std::string &msg, const bool duplicate_check = false);
    virtual void LogVerb1(const std::string &msg, const bool duplicate_check = false);
    virtual void LogInfo(const std::string &msg, const bool duplicate_check = false);
    virtual void LogWarn(const std::string &msg, const bool duplicate_check = false);
    virtual void LogError(const std::string &msg);
    virtual void LogCritical(const std::string &msg);
    virtual void LogFATAL(const std::string &msg);
    LogEvent GetLastLogEvent() const;

    LogWriter *GetLogWriter();


  protected:
    LogWriter *logwr = nullptr;
    LogGroup log_group;


  private:
    LogEvent last_logevent;
};



class LogConsumer : public LogFilter
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
                                 const LogEvent &logev) = 0;
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
        LogEvent logev(event->params);

        if (!LogFilterAllow(logev))
        {
            return;
        }
        ConsumeLogEvent(event->sender,
                        event->object_interface,
                        event->object_path,
                        logev);
    }

  private:
    DBus::Signals::SubscriptionManager::Ptr subscriptions = nullptr;
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
                         const LogEvent &logev)
    {
        // This is a dummy method and is not used by LogConsumerProxy.
        // The InterceptLogEvent() method is used instead, which allows
        // the LogEvent to be modified on-the-fly _before_ being proxied.
    }


    // clang-format off
    // bug in clang-format causes "= 0" to be wrapped
    virtual LogEvent InterceptLogEvent(const std::string &sender,
                                       const std::string &interface,
                                       const std::string &object_path,
                                       const LogEvent &logev) = 0;
    // clang-format on


  protected:
    virtual void process_log_event(DBus::Signals::Event::Ptr event);
};
