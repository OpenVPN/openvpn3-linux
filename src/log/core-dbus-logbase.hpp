//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//

/**
 * @file   core-dbus-logbase.hpp
 *
 * @brief  Core library log implementation facilitating
 *         the D-Bus logging infrastructure in the Linux client
 */

#pragma once

#include <iostream>
#include <algorithm>

#include <openvpn/log/logbase.hpp>

#include "dbus/core.hpp"
#include "log/dbus-log.hpp"


namespace openvpn {

/**
 *  Log implementation for the Core library.  This class will
 *  be used by some mmagic in the OpenVPN 3 Core library
 *  when this object is instantiated in the main program.
 *
 *  Logging from the Core library is quite low-level and since
 *  the logging interface in the library does not define any log
 *  levels, these events will be handled as debug log messages.
 */
class CoreDBusLogBase : public LogBase,
                        public LogSender
{
  public:
    typedef RCPtr<CoreDBusLogBase> Ptr;

    /**
     *  Initializes the Core library D-Bus logging
     *
     * @param dbuscon   GDBusConnection where to send Log signals
     * @param interface String containing the interface messages will be
     *                  tagged with
     * @param lgrp      LogGroup to use when sending log events
     * @param logwr     LogWriter object to handle local logging
     */
    CoreDBusLogBase(GDBusConnection *dbuscon,
                    std::string interface,
                    LogGroup lgrp,
                    LogWriter *logwr)
        : LogSender(dbuscon, lgrp, interface, OpenVPN3DBus_rootp_netcfg, logwr),
          log_context(this)
    {
        SetLogLevel(6);
        Debug("OpenVPN 3 Core library logging initialized");
    }


    virtual void log(const std::string &str) override
    {
        std::string l(str);
        l.erase(l.find_last_not_of(" \n") + 1); // rtrim
        Debug("[Core] " + l);
    }

  private:
    Log::Context log_context;
};
} // namespace openvpn
