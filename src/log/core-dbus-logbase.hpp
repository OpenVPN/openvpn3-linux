//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
                    std::string interface, LogGroup lgrp,
                    LogWriter *logwr)
        : LogSender(dbuscon, lgrp,
                    interface,
                    OpenVPN3DBus_rootp_netcfg, logwr),
          log_context(this)
    {
        SetLogLevel(6);
        Debug("OpenVPN 3 Core library logging initialized");
    }

    virtual void log(const std::string& str) override
    {
        std::string l(str);
        l.erase(l.find_last_not_of(" \n")+1); // rtrim
        Debug("[Core] " + l);
    }

private:
    Log::Context log_context;
};
} // namespace openvpn
