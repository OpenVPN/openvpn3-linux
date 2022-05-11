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

#pragma once

#include <fstream>
#include <ctime>
#include <exception>
#include <string>

#include "dbus/core.hpp"
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
    LogFilter(unsigned int log_level_val) noexcept;
    ~LogFilter() = default;

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
    void SetLogLevel(unsigned int loglev);

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
    void AddPathFilter(const std::string& path);


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
    bool LogFilterAllow(const LogEvent& logev) noexcept;


    /**
     *  Checks if the path is on the path list configured via @AddPathFilter.
     *
     * @param path  std::string of path to check
     * @return Returns true if the path is allowed. If no paths has been added,
     *         it will always return true.
     */
    bool AllowPath(const std::string& path) noexcept;


private:
    unsigned int log_level;
    std::vector<std::string> filter_paths;
};


class LogSender : public DBusSignalProducer,
                  public LogFilter
{
public:
    LogSender(GDBusConnection * dbuscon, const LogGroup lgroup,
              std::string interf, std::string objpath,
              LogWriter *lgwr = nullptr);
    virtual ~LogSender() = default;

    virtual const std::string GetLogIntrospection();
    const std::string GetStatusChangeIntrospection();

    void StatusChange(const StatusEvent& statusev);

    void ProxyLog(const LogEvent& logev, const std::string& path = "");
    void ProxyStatusChange(const StatusEvent& status, const std::string& path);

    virtual void Log(const LogEvent& logev, bool duplicate_check = false,
                     const std::string& target ="");
    virtual void Debug(std::string msg, bool duplicate_check = false);
    virtual void LogVerb2(std::string msg, bool duplicate_check = false);
    virtual void LogVerb1(std::string msg, bool duplicate_check = false);
    virtual void LogInfo(std::string msg, bool duplicate_check = false);
    virtual void LogWarn(std::string msg, bool duplicate_check = false);
    virtual void LogError(std::string msg);
    virtual void LogCritical(std::string msg);
    virtual void LogFATAL(std::string msg);
    LogEvent GetLastLogEvent() const;

    LogWriter * GetLogWriter();

protected:
    LogWriter *logwr = nullptr;
    LogGroup log_group;

private:
    LogEvent last_logevent;
};


class LogConsumer : public DBusSignalSubscription,
                    public LogFilter
{
public:
    LogConsumer(GDBusConnection *dbuscon,
                std::string interf,
                std::string objpath,
                std::string busn = "");
    ~LogConsumer() = default;

    virtual void ConsumeLogEvent(const std::string sender,
                                 const std::string interface_name,
                                 const std::string obj_path,
                                 const LogEvent& logev) = 0;


    virtual void ProcessSignal(const std::string sender_name,
                               const std::string obj_path,
                               const std::string interface_name,
                               const std::string signal_name,
                               GVariant *parameters)
    {
    }

    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string obj_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters);
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
    virtual void process_log_event(const std::string sender,
                                   const std::string interface_name,
                                   const std::string obj_path,
                                   GVariant *params)
    {
        // Pass the signal parameters to be parsed by LogEvent directly
        LogEvent logev(params);

        if (!LogFilterAllow(logev))
        {
            return;
        }
        ConsumeLogEvent(sender, interface_name, obj_path, logev);
    }
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
                              const std::string& message_str) noexcept;
    ~LogConsumerProxyException() = default;

    const char* what() const noexcept;
    LogProxyExceptionType GetExceptionType() const noexcept;

private:
    LogProxyExceptionType type;
    std::string message;
};



class LogConsumerProxy : public LogConsumer, public LogSender
{
public:
    LogConsumerProxy(GDBusConnection *dbuscon,
                     std::string src_interf, std::string src_objpath,
                     std::string dst_interf, std::string dst_objpath);
    ~LogConsumerProxy() = default;

    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogEvent& logev)
    {
        // This is a dummy method and is not used by LogConsumerProxy.
        // The InterceptLogEvent() method is used instead, which allows
        // the LogEvent to be modified on-the-fly _before_ being proxied.
    }

    virtual LogEvent InterceptLogEvent(const std::string sender,
                                       const std::string interface,
                                       const std::string object_path,
                                       const LogEvent& logev) = 0;

protected:
    virtual void process_log_event(const std::string sender,
                                   const std::string interface,
                                   const std::string object_path,
                                   GVariant *params);
};
