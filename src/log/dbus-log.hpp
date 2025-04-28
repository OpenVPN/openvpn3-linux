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
              LogWriter *lgwr = nullptr);
    virtual ~LogSender() = default;

    const LogGroup GetLogGroup() const;

    virtual void Log(const Events::Log &logev, const bool duplicate_check = false, const std::string &target = "");
    virtual void Debug(const std::string &msg, const bool duplicate_check = false);
    virtual void Debug_wnl(const std::string &msg, const bool duplicate_check = false);
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
