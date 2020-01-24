//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017 - 2019  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017 - 2019  David Sommerseth <davids@openvpn.net>
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

#ifndef OPENVPN3_DBUS_LOG_HPP
#define OPENVPN3_DBUS_LOG_HPP

#include <fstream>
#include <ctime>
#include <exception>

#include "dbus/signals.hpp"
#include "client/statusevent.hpp"
#include "log-helpers.hpp"
#include "logevent.hpp"
#include "logwriter.hpp"

namespace openvpn
{
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
        LogFilter(unsigned int log_level)
            : log_level(log_level)
        {
        }


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
        void SetLogLevel(unsigned int loglev)
        {
            if (loglev > 6)
            {
                THROW_LOGEXCEPTION("LogSender: Invalid log level");
            }
            log_level = loglev;
        }


        /**
         * Retrieves the log level in use
         *
         * @return unsigned int, with values between 0-6
         *
         */
        unsigned int GetLogLevel()
        {
            return log_level;
        }


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
        bool LogFilterAllow(const LogEvent& logev)
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


    private:
        unsigned int log_level;
    };


    class LogSender : public DBusSignalProducer,
                      public LogFilter
    {
    public:
        LogSender(GDBusConnection * dbuscon, const LogGroup lgroup,
                  std::string interf, std::string objpath,
                  LogWriter *logwr = nullptr)
            : DBusSignalProducer(dbuscon, "", interf, objpath),
              LogFilter(3),
              logwr(logwr),
              log_group(lgroup)
        {
        }

        virtual ~LogSender() = default;


        virtual const std::string GetLogIntrospection()
        {
            return
                "        <signal name='Log'>"
                "            <arg type='u' name='group' direction='out'/>"
                "            <arg type='u' name='level' direction='out'/>"
                "            <arg type='s' name='message' direction='out'/>"
                "        </signal>";
        }

        const std::string GetStatusChangeIntrospection()
        {
            return
                "        <signal name='StatusChange'>"
                "            <arg type='u' name='code_major' direction='out'/>"
                "            <arg type='u' name='code_minor' direction='out'/>"
                "            <arg type='s' name='message' direction='out'/>"
                "        </signal>";
        }


        void StatusChange(const StatusEvent& statusev)
        {
            Send("StatusChange", statusev.GetGVariantTuple());
        }

        void ProxyLog(const LogEvent& logev)
        {
            // Don't proxy this log message unless the log level filtering
            // allows it.  The filtering is done against the LogCategory of
            // the message, so we need to extract the LogCategory first
            if (LogFilterAllow(logev))
            {
                Send("Log", logev.GetGVariantTuple());
            }
        }

        virtual void Log(const LogEvent& logev)
        {
            // Don't log unless the log level filtering allows it
            // The filtering is done against the LogCategory of the message
            if (!LogFilterAllow(logev))
            {
                return;
            }

            if( logwr )
            {
                logwr->Write(logev);
            }
            Send("Log", g_variant_new("(uus)",
                                      (guint) logev.group,
                                      (guint) logev.category,
                                      logev.message.c_str()));
        }

        virtual void Debug(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::DEBUG, msg));
        }

        virtual void LogVerb2(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::VERB2, msg));
        }

        virtual void LogVerb1(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::VERB1, msg));
        }

        virtual void LogInfo(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::INFO, msg));
        }

        virtual void LogWarn(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::WARN, msg));
        }

        virtual void LogError(std::string msg)
        {
            Log(LogEvent(log_group, LogCategory::ERROR, msg));
        }

        virtual void LogCritical(std::string msg)
        {
            // Critical log messages will always be sent
            Log(LogEvent(log_group, LogCategory::CRIT, msg));
        }

        virtual void LogFATAL(std::string msg)
        {
            // Fatal log messages will always be sent
            Log(LogEvent(log_group, LogCategory::FATAL, msg));
            // FIXME: throw something here, to start shutdown procedures
        }


        LogWriter * GetLogWriter()
        {
            return logwr;
        }


    protected:
        LogWriter *logwr = nullptr;
        LogGroup log_group;
    };


    class LogConsumer : public DBusSignalSubscription,
                        public LogFilter
    {
    public:
        LogConsumer(GDBusConnection * dbuscon, std::string interf,
                    std::string objpath, std::string busn = "")
            : DBusSignalSubscription(dbuscon, busn, interf, objpath, "Log"),
              LogFilter(6)  // By design, accept all kinds of log messages when receiving
        {
        }

        virtual void ConsumeLogEvent(const std::string sender,
                                     const std::string interface,
                                     const std::string object_path,
                                     const LogEvent& logev) = 0;


        virtual void ProcessSignal(const std::string sender_name,
                                   const std::string object_path,
                                   const std::string interface_name,
                                   const std::string signal_name,
                                   GVariant *parameters)
        {
        }

        void callback_signal_handler(GDBusConnection *connection,
                                     const std::string sender_name,
                                     const std::string object_path,
                                     const std::string interface_name,
                                     const std::string signal_name,
                                     GVariant *parameters)
        {
            if ("Log" == signal_name)
            {
                process_log_event(sender_name, interface_name, object_path,
                                  parameters);
            }
            else
            {
                ProcessSignal(sender_name, object_path, interface_name,
                              signal_name, parameters);
            }
        }

    protected:
        virtual void process_log_event(const std::string sender,
                                       const std::string interface,
                                       const std::string object_path,
                                       GVariant *params)
        {
            LogEvent logev(params);

            if (!LogFilterAllow(params))
            {
                return;
            }
            ConsumeLogEvent(sender, interface, object_path, logev);
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
        LogConsumerProxyException(LogProxyExceptionType type) noexcept
            : type(type), message()
        {
        }

        LogConsumerProxyException(LogProxyExceptionType type,
                                  const std::string& message) noexcept
            : type(type), message(std::move(message))
        {
        }

        ~LogConsumerProxyException() = default;

        const char *what() const noexcept
        {
            return message.c_str();
        }

        LogProxyExceptionType GetExceptionType() const noexcept
        {
            return type;
        }

    private:
        LogProxyExceptionType type;
        std::string message;
    };



    class LogConsumerProxy : public LogConsumer, public LogSender
    {
    public:
        LogConsumerProxy(GDBusConnection *dbuscon,
                         std::string src_interf, std::string src_objpath,
                         std::string dst_interf, std::string dst_objpath)
            : LogConsumer(dbuscon, src_interf, src_objpath),
              LogSender(dbuscon, LogGroup::UNDEFINED, dst_interf, dst_objpath)
        {
        }

        void ConsumeLogEvent(const std::string sender,
                             const std::string interface,
                             const std::string object_path,
                             const LogEvent& logev)
        {
            // This is a dummy method and is not used by LogConsumerProxy.
            // The InterceptLogEvent() method is used instead, which allows
            // the LogEvent to be modified on-the-fly before being proxied.
        }

        virtual LogEvent InterceptLogEvent(const std::string sender,
                                           const std::string interface,
                                           const std::string object_path,
                                           const LogEvent& logev) = 0;

    protected:
        virtual void process_log_event(const std::string sender,
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
                    // The InterceptLogEvent method asks is to ignore and not
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
    };
};

#endif // OPENVPN3_DBUS_LOG_HPP
