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
#include <iomanip>

#include "log-helpers.hpp"

namespace openvpn
{
    std::string GetTimestamp()
    {
        time_t now = time(0);
        tm *ltm = localtime(&now);

        std::stringstream ret;
        ret << 1900 + ltm->tm_year
            << "-" << std::setw(2) << std::setfill('0') << 1 + ltm->tm_mon
            << "-" << std::setw(2) << std::setfill('0') << ltm->tm_mday
            << " " << std::setw(2) << std::setfill('0') << ltm->tm_hour
            << ":" << std::setw(2) << std::setfill('0') << ltm->tm_min
            << ":" << std::setw(2) << std::setfill('0') << ltm->tm_sec
            << " ";
        return ret.str();
    }

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

        virtual void LogWrite(const std::string sender,
                              LogGroup lgroup, LogCategory catg,
                              std::string msg) final
        {
            if( !file_open )
            {
                THROW_LOGEXCEPTION("FileLog: No log file opened");
            }
            logfs << GetTimestamp();
            if (!sender.empty())
            {
                  logfs << "[" << sender << "]";
            }
            logfs << LogPrefix(lgroup, catg)
                  << msg
                  << std::endl;
            logfs.flush();
        }

        virtual void LogWrite(const std::string sender,
                              guint32 lgroup, guint32 catg,
                              gchar *msg) final
        {
            LogWrite(sender, (LogGroup) lgroup, (LogCategory) catg,
                     std::string(msg));
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
                      public FileLog
    {
    public:
        LogSender(GDBusConnection * dbuscon, const LogGroup lgroup, std::string interf, std::string objpath)
            : DBusSignalProducer(dbuscon, "", interf, objpath),
              FileLog(),
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
            Send("Log", values);
        }

        void Log(const LogGroup group, const LogCategory catg, const std::string msg)
        {
            if( GetLogActive() )
            {
                LogWrite("", group, catg, msg);
            }
            guint gr = (guint) group;
            guint cg = (guint) catg;
            Send("Log", g_variant_new("(uus)", gr, cg, msg.c_str()));
        }

        virtual void Debug(std::string msg)
        {
            Log(log_group, LogCategory::DEBUG, msg);
        }

        virtual void LogVerb2(std::string msg)
        {
            Log(log_group, LogCategory::VERB2, msg);
        }

        virtual void LogVerb1(std::string msg)
        {
            Log(log_group, LogCategory::VERB1, msg);
        }

        virtual void LogInfo(std::string msg)
        {
            Log(log_group, LogCategory::INFO, msg);
        }

        virtual void LogWarn(std::string msg)
        {
            Log(log_group, LogCategory::WARN, msg);
        }

        virtual void LogError(std::string msg)
        {
            Log(log_group, LogCategory::ERROR, msg);
        }

        virtual void LogCritical(std::string msg)
        {
            Log(log_group, LogCategory::CRIT, msg);
        }

        virtual void LogFATAL(std::string msg)
        {
            Log(log_group, LogCategory::FATAL, msg);
            // FIXME: throw something here, to start shutdown procedures
        }

    protected:
        LogGroup log_group;

    };


    class LogConsumer : public DBusSignalSubscription,
                        public FileLog
    {
    public:
        LogConsumer(GDBusConnection * dbuscon, std::string interf, std::string objpath)
            : DBusSignalSubscription(dbuscon, "", interf, "", "Log"),
              FileLog()
        {
        }

        virtual void ConsumeLogEvent(const std::string sender, const std::string interface, const std::string object_path,
                                     const LogGroup group, const LogCategory catg, const std::string msg) = 0;

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

            if (GetLogActive())
            {
                LogWrite(sender, group, catg, msg);
            }
            ConsumeLogEvent(sender, interface, object_path, (LogGroup) group, (LogCategory) catg, std::string(msg));
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
                                     const LogGroup group,
                                     const LogCategory catg,
                                     const std::string msg) = 0;

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

            if (openvpn::LogConsumer::GetLogActive())
            {
                openvpn::LogConsumer::LogWrite(sender, group, catg, msg);
            }
            ConsumeLogEvent(sender, interface, object_path,
                            (LogGroup) group, (LogCategory) catg, std::string(msg));
            ProxyLog(params);
            g_free(msg);
        }
    };
};

#endif // OPENVPN3_DBUS_LOG_HPP
