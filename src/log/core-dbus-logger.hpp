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

#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <gdbuspp/connection.hpp>

#include "log/dbus-log.hpp"


/**
 *  OpenVPN 3 Linux logging implementation for OpenVPN 3 Core library
 */
namespace CoreLog {

/**
 *  Initializes the Core library D-Bus logging
 *
 * @param dbuscon   DBus::Connection where to send Log signals
 * @param path      String containing the sending D-Bus path
 * @param interface String containing the interface messages will be
 *                  tagged with
 * @param lgrp      LogGroup to use when sending log events
 * @param session_token String containing the session token to attach to the log messages
 * @param logwr     LogWriter object to handle local logging
 */
void Initialize(DBus::Connection::Ptr dbuscon,
                const std::string &path,
                const std::string &interface,
                const LogGroup lgrp,
                const std::string session_token,
                LogWriter *logwr);

/**
 *  This is an alternative way to initialize the Core D-Bus logging.
 *  If a LogSender object is already prepared for logging, the Core logger
 *  can make use of this logging object instead of creating its own.
 *
 * @param log_object  LogSender::Ptr object to use for logging
 */
void Connect(LogSender::Ptr log_object);

/**
 *  Changes the log level of the application.
 *
 * @param log_level
 */
void SetLogLevel(const uint8_t log_level);


/**
 *  Internal helper function, used by the OPENVPN_LOG(), OPENVPN_LOG_NTNL()
 *  and OPENVPN_LOG_STRING() macros.  The Core library does all the logging
 *  via these macros, and these macros will be using this internal function
 *  when doing logging operations.
 *
 * @param logmsg  std::string of the log message to log
 */
void ___core_log(const std::string &logmsg);


//  Declares the global core logger object used by the Core logger
class DBusLogger;
extern std::shared_ptr<DBusLogger> ___globalLog;
} // namespace CoreLog


//
//  Below is the mandatory macros and context structure which will be
//  used by the OpenVPN 3 Core library when defined before any of the
//  OpenVPN 3 Core library headers will be included.
//
//  The concept here is based on the code found in
//  openvpn3-core/openvpn/log/logsimple.hpp
//

#define OPENVPN_LOG(msg)                \
    {                                   \
        std::ostringstream ls;          \
        ls << msg;                      \
        CoreLog::___core_log(ls.str()); \
    }

#define OPENVPN_LOG_NTNL(msg)           \
    {                                   \
        std::ostringstream ls;          \
        ls << msg;                      \
        CoreLog::___core_log(ls.str()); \
    }

#define OPENVPN_LOG_STRING(str) CoreLog::___core_log(str)


// no-op constructs normally used with logthread.hpp
namespace openvpn {
namespace Log {
struct Context
{
    struct Wrapper
    {
    };
    Context(const Wrapper &)
    {
    }
};
} // namespace Log
} // namespace openvpn
