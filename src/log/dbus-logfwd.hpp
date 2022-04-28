//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2022         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2022         David Sommerseth <davids@openvpn.net>
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
 * @file   dbus-logfwd.hpp
 *
 * @brief  The LogForwardBase implementation is the generic code used to
 *         subscribe to log events from specific senders directly from
 *         the net.openvpn.v3.log service
 */

#pragma once

#include <memory>
#include <string>

#include "dbus-log.hpp"
#include "proxy-log.hpp"

template <typename C>
using LogFwdProcessor = std::shared_ptr<C>;


/**
 *  The LogForwardBase is a helper class, setting up a LogServiceProxy and
 *  LogConsumer and setup everything related for the ProxyLogEvent method call.
 *
 *  This class will also ensure LogProxy object created by the ProxyLogEvent
 *  is correctly removed when the class implementing LogForwardBase is torn
 *  down.
 *
 *  The implementing class need to implement the @ConsumeLogEvent method
 *  as well as @ProcessSignal for handling StatusChange events.
 *
 *  @tparam C   The class name of the class implementing LogForwardBase
 */
template<typename C>
class LogForwardBase : public LogConsumer,
                       public LogServiceProxy,
                       public std::enable_shared_from_this<LogForwardBase<C>>
{
public:
    /**
     *  Helper method to create a std::shared_ptr<> of the implementing class
     *
     * @tparam T     The T template is only used to pass arbitary arguments
     *               to the implementing class constructor.
     * @param args   The normal arbitrary arguments to the implementing class
     *               constructor
     * @return       Returns a LogFwdProcessor (aka std::shared_ptr<C>) of
     *               the created object.
     */
    template<typename ... T>
    static LogFwdProcessor<C> create(T&& ... args)
    {
        return LogFwdProcessor<C>(new C(std::forward<T>(args)...));
    }


    /**
     *  Retrieves the D-Bus path to the log proxy object in the
     *  net.openpvn.v3.log service.
     */
    const std::string GetLogProxyPath() const
    {
        return logproxy->GetPath();
    }

    /**
     *  Retrieves the D-Bus path to the VPN session D-Bus object this log
     *  forwarding/proxy object is tied to.
     */
    const std::string GetSessionPath() const
    {
        return logproxy->GetSessionPath();
    }


    /**
     *  Retrieve the log level of the log events being forwarded.
     */
    const unsigned int GetLogLevel() const
    {
        return logproxy->GetLogLevel();
    }


    /**
     *  Retrieve the D-Bus unique bus name of the client owning this
     *  log forward/proxy object
     */
    const std::string GetLogTarget() const
    {
        return logproxy->GetLogTarget();
    }


protected:
    LogForwardBase(GDBusConnection *dbusc,
                   const std::string& target,
                   const std::string& interf,
                   const std::string& session_path)
       : LogConsumer(dbusc, interf, session_path, ""),
         LogServiceProxy(dbusc)
    {
        logproxy = LogServiceProxy::ProxyLogEvents(target,
                                                   session_path);
        Subscribe(session_path, "StatusChange");
    }

    ~LogForwardBase()
    {
        if (logproxy)
        {
            logproxy->Remove();
        }
    }


private:
    LogProxy::Ptr logproxy;
};
