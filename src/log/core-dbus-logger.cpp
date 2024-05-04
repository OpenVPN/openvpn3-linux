//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   core-dbus-logbase.hpp
 *
 * @brief  Core library log implementation facilitating
 *         the D-Bus logging infrastructure in the Linux client
 */

#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/signals/group.hpp>

#include "dbus/constants.hpp"
#include "log/dbus-log.hpp"
#include "core-dbus-logger.hpp"


namespace CoreLog {

class DBusLogger
{
  public:
    using Ptr = std::shared_ptr<DBusLogger>;

    /**
     *  Initializes the Core library D-Bus logging
     *
     * @param dbuscon   DBus::Connection where to send Log signals
     * @param path      String containing the sending D-Bus path
     * @param interface String containing the interface messages will be
     *                  tagged with
     * @param lgrp      LogGroup to use when sending log events
     * @param logwr     LogWriter object to handle local logging
     */
    [[nodiscard]] static DBusLogger::Ptr Create(
        DBus::Connection::Ptr dbuscon,
        const std::string &path,
        const std::string &interface,
        const LogGroup lgrp,
        const std::string session_token,
        LogWriter *logwr)
    {
        return Ptr(new DBusLogger(dbuscon,
                                  path,
                                  interface,
                                  lgrp,
                                  session_token,
                                  logwr));
    }


    /**
     *  Initializes the Core library D-Bus logging using an existing
     *  LogSender object
     *
     * @param
     */
    [[nodiscard]] static DBusLogger::Ptr Create(LogSender::Ptr log_obj)
    {
        return Ptr(new DBusLogger(log_obj));
    }


    ~DBusLogger() noexcept = default;


    void SetLogLevel(const uint8_t log_level)
    {
        logger->SetLogLevel(log_level);
    }


    void log(const std::string &str) const noexcept
    {
        std::string l(str);
        try
        {
            l.erase(l.find_last_not_of(" \n") + 1); // rtrim

            if (session_token.empty())
            {
                logger->Log(Events::Log(log_group,
                                        LogCategory::DEBUG,
                                        "[Core] " + l));
            }
            else
            {
                logger->Log(Events::Log(log_group,
                                        LogCategory::DEBUG,
                                        session_token,
                                        "[Core] " + l));
            }
        }
        catch (const DBus::Signals::Exception &)
        {
            std::cout << "{ERROR: D-Bus Signal Sending Failed} "
                      << "[Core] " << l << std::endl;
        }
    }


  private:
    LogSender::Ptr logger = nullptr;
    const LogGroup log_group;
    const std::string session_token;


    DBusLogger(DBus::Connection::Ptr dbuscon,
               const std::string &path,
               const std::string &interface,
               const LogGroup lgrp,
               const std::string session_token_,
               LogWriter *logwr)
        : log_group(lgrp), session_token(session_token_)
    {
        logger = DBus::Signals::Group::Create<LogSender>(dbuscon,
                                                         lgrp,
                                                         path,
                                                         interface,
                                                         !session_token_.empty(),
                                                         logwr);

        auto qry = DBus::Proxy::Utils::DBusServiceQuery::Create(dbuscon);
        logger->AddTarget(qry->GetNameOwner(Constants::GenServiceName("log")));
        logger->SetLogLevel(6);
        log("OpenVPN 3 Core library logging initialized");
    }

    DBusLogger(LogSender::Ptr log_obj)
        : log_group(log_obj->GetLogGroup())
    {
        logger = log_obj;
    }
};

DBusLogger::Ptr ___globalLog = nullptr;


void Initialize(DBus::Connection::Ptr dbuscon,
                const std::string &path,
                const std::string &interface,
                const LogGroup lgrp,
                const std::string session_token,
                LogWriter *logwr)
{
    if (___globalLog)
    {
        return;
    }
    ___globalLog = DBusLogger::Create(dbuscon,
                                      path,
                                      interface,
                                      lgrp,
                                      session_token,
                                      logwr);
}

void Connect(LogSender::Ptr log_object)
{
    if (___globalLog)
    {
        return;
    }
    ___globalLog = DBusLogger::Create(log_object);
}

void SetLogLevel(const uint8_t log_level)
{
    if (___globalLog)
    {
        ___globalLog->SetLogLevel(log_level);
    }
    else
    {
        std::cout << "[CoreLog (fallback)]  No D-Bus logger initialized, "
                  << "ignoring SetLogLevel" << std::endl;
    }
}


void ___core_log(const std::string &logmsg)
{
    if (___globalLog)
    {
        std::ostringstream ls;
        ls << logmsg;
        ___globalLog->log(ls.str());
    }
    else
    {
        std::cout << "[CoreLog (fallback)] " << logmsg << std::endl;
    }
}

} // namespace CoreLog
