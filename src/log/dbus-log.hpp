//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

#include <gio/gio.h>
#include "dbus/signals.hpp"
#include "log-helpers.hpp"

namespace openvpn
{
    /**
     *  Basic Log Event container
     */
    struct LogEvent
    {
        /**
         *  Initializes an empty LogEvent struct.
         */
        LogEvent()
        {
            reset();
        }

        /**
         *  Initialize the LogEvent object with the provided details.
         *
         * @param grp  LogGroup value to use.
         * @param ctg  LogCategory value to use.
         * @param msg  std::string containing the log message to use.
         */
        LogEvent(const LogGroup grp, const LogCategory ctg,
                 const std::string msg)
            : group(grp), category(ctg), message(msg)
        {
        }

        /**
         *  Initialize a LogEvent, based on a GVariant object containing
         *  a log entry.  See @Parse() for more information.
         *
         * @param status
         */
        LogEvent(GVariant *logev)
        {
            reset();
            Parse(logev);
        }


        /**
         *  Resets the LogEvent struct to a known and empty state
         */
        void reset()
        {
            group = LogGroup::UNDEFINED;
            category = LogCategory::UNDEFINED;
            message = "";
        }


        /**
         *  Checks if the LogEvent object is empty
         *
         * @return Returns true if it is empty/unused
         */
        bool empty()
        {
            return (LogGroup::UNDEFINED == group)
                   && (LogCategory::UNDEFINED == category)
                   && message.empty();
        }

        /**
         *  Parses a GVariant object containing a Log signal.  The input
         *  GVariant needs to be of 'a{sv}' which is a named dictonary.  It
         *  must contain the following key values to be valid:
         *
         *     - (u) log_group       Translated into LogGroup
         *     - (u) log_category    Translated into LogCategory
         *     - (s) log_message     A string with the log message
         *
         * @param logevent  Pointer to the GVariant object containig the
         *                  log event
         */
        void Parse(GVariant *logevent)
        {
            GVariant *d = nullptr;
            unsigned int v = 0;

            d = g_variant_lookup_value(logevent, "log_group", G_VARIANT_TYPE_UINT32);
            v = g_variant_get_uint32(d);
            if (v > 0 && v < LogGroup_str.size())
            {
                group = (LogGroup) v;
            }
            g_variant_unref(d);

            d = g_variant_lookup_value(logevent, "log_category", G_VARIANT_TYPE_UINT32);
            v = g_variant_get_uint32(d);
            if (v > 0 && v < LogCategory_str.size())
            {
                category = (LogCategory) v;
            }
            g_variant_unref(d);

            gsize len;
            d = g_variant_lookup_value(logevent,
                                       "log_message", G_VARIANT_TYPE_STRING);
            message = std::string(g_variant_get_string(d, &len));
            g_variant_unref(d);
            if (len != message.size())
            {
                THROW_LOGEXCEPTION("Failed retrieving log event message text"
                                   " (inconsistent length)");
            }
        }


        /**
         *  Makes it possible to write LogEvents in a readable format
         *  via iostreams, such as 'std::cout << event', where event is a
         *  LogEvent object.
         *
         * @param os  std::ostream where to write the data
         * @param ev  LogEvent to write to the stream
         *
         * @return  Returns the provided std::ostream together with the
         *          decoded LogEvent information
         */
        friend std::ostream& operator<<(std::ostream& os , const LogEvent& ev)
        {
            return os << LogPrefix(ev.group, ev.category) << ev.message;
        }

        LogGroup group;
        LogCategory category;
        std::string message;
    };



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
         * @param catg  LogCategory value to check against the set log level
         *
         * @return  Returns true if this Log Category should be logged
         */
        bool LogFilterAllow(LogCategory catg)
        {
            switch(catg)
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


        /**
         * Checks if the LogCategory matches a log level where
         * logging should happen
         *
         * @param catg  Unsigned integer representation of the LogCategory
         *              value to check against the set log level
         *
         * @return  Returns true if this LogCategory should be logged
         */
        bool LogFilterAllow(guint catg)
        {
            return LogFilterAllow((LogCategory) catg);
        }

    private:
        unsigned int log_level;
};


    class FileLog
    {
    public:
        FileLog()
            : file_open(false)
        {
        }

        ~FileLog()
        {
            if (file_open)
            {
                // FIXME: Close file
            }
        }

        virtual void OpenLogFile(std::string filename)
        {
            if (file_open)
            {
                THROW_LOGEXCEPTION("FileLog: Log file already opened");
            }
            logfs.open(filename, std::ios_base::app);
            if (!logfs)
            {
                THROW_LOGEXCEPTION("FileLog: Failed to open logfile '" + filename + "'");
            }
            file_open = true;
        }


        virtual void LogWrite(const std::string sender, const LogEvent& logev) final
        {
            if( !file_open )
            {
                THROW_LOGEXCEPTION("FileLog: No log file opened");
            }
            logfs << GetTimestamp();
            if (!sender.empty())
            {
                  logfs << "[" << sender << "] ";
            }
            logfs << logev << std::endl;
            logfs.flush();
        }


        bool GetLogActive()
        {
            return file_open;
        }

    private:
        bool file_open;
        std::ofstream logfs;
    };


    class LogSender : public DBusSignalProducer,
                      public FileLog,
                      public LogFilter
    {
    public:
        LogSender(GDBusConnection * dbuscon, const LogGroup lgroup, std::string interf, std::string objpath)
            : DBusSignalProducer(dbuscon, "", interf, objpath),
              FileLog(),
              LogFilter(3),
              log_group(lgroup)
        {
        }

        virtual ~LogSender()
        {
        }

        const std::string GetLogIntrospection()
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

        void ProxyLog(GVariant *values)
        {
            // Don't proxy this log message unless the log level filtering
            // allows it.  The filtering is done against the LogCategory of
            // the message, so we need to extract the LogCategory first

            guint group = 0;
            guint catg = 0;
            g_variant_get(values, "(uus)", &group, &catg, NULL);

            if (LogFilterAllow(catg))
            {
                Send("Log", values);
            }
        }

        void Log(const LogEvent& logev)
        {
            // Don't log unless the log level filtering allows it
            // The filtering is done against the LogCategory of the message
            if (!LogFilterAllow(logev.category))
            {
                return;
            }

            if( GetLogActive() )
            {
                LogWrite("", logev);
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


    protected:
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

        void callback_signal_handler(GDBusConnection *connection,
                                     const std::string sender_name,
                                     const std::string object_path,
                                     const std::string interface_name,
                                     const std::string signal_name,
                                     GVariant *parameters)
        {
            process_log_event(sender_name, interface_name, object_path, parameters);
        }

    protected:
        virtual void process_log_event(const std::string sender,
                                       const std::string interface,
                                       const std::string object_path,
                                       GVariant *params)
        {
            guint group;
            guint catg;
            gchar *msg;
            g_variant_get (params, "(uus)", &group, &catg, &msg);
            auto logev = LogEvent((LogGroup) group, (LogCategory) catg,
                                  std::string(msg));

            if (!LogFilterAllow(catg))
            {
                goto exit;
            }
            ConsumeLogEvent(sender, interface, object_path, logev);

        exit:
            g_free(msg);
        }
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

        virtual void ConsumeLogEvent(const std::string sender,
                                     const std::string interface,
                                     const std::string object_path,
                                     const LogEvent& logev) = 0;

    protected:
        virtual void process_log_event(const std::string sender,
                                       const std::string interface,
                                       const std::string object_path,
                                       GVariant *params)
        {
            guint group;
            guint catg;
            gchar *msg;
            g_variant_get (params, "(uus)", &group, &catg, &msg);
            auto logev = LogEvent((LogGroup) group, (LogCategory) catg,
                                  std::string(msg));

            ConsumeLogEvent(sender, interface, object_path, logev);
            ProxyLog(params);
            g_free(msg);
        }
    };
};

#endif // OPENVPN3_DBUS_LOG_HPP
