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
#include "sessionmgr/proxy-sessionmgr.hpp"

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
     *  Retrieve the log level of the log events being forwarded.
     */
    const unsigned int GetLogLevel() const
    {
        return session_proxy->GetUIntProperty("log_verbosity");
    }


    /**
     *  The LogForwardBase also listens for StatusChange events.  These are
     *  already parsed to set some internal statuses before it can be handled
     *  by an implementation.  Once that has happened, this method is called
     *  as a callback function with the parsed status.
     *
     *  The default implementation in LogForwardBase is to let them pass
     *  without any processing.
     *
     * @param sender_name
     * @param obj_path
     * @param interface_name
     * @param status           StatusEvent object of the event received
     */
    virtual void StatusChangeEvent(const std::string sender_name,
                                   const std::string interface_name,
                                   const std::string obj_path,
                                   const StatusEvent& status)
    {
    }


    /**
     *  All signals which is sent to the D-Bus client, with StatusChange and
     *  Log events as the exceptions  will be passed on to this method.  The
     *  default is to let them pass without any processing.
     *
     *  This method replaces the ProcessSignal() method otherwise used in
     *  LogConsumer.  This is not used here, as the LogForwardBase captuers
     *  this processing before this method is called.
     *
     * @param sender_name
     * @param obj_path
     * @param interface_name
     * @param signal_name
     * @param parameters
     */
    virtual void SignalHandler(const std::string sender_name,
                               const std::string obj_path,
                               const std::string interface_name,
                               const std::string signal_name,
                               GVariant *parameters)
    {
    }


protected:
    LogForwardBase(DBus& dbusc,
                   const std::string& interf,
                   const std::string& session_path)
       : LogConsumer(dbusc.GetConnection(), interf, session_path, "")
    {
        Subscribe(session_path, "StatusChange");
        session_proxy.reset(new OpenVPN3SessionProxy(dbusc, session_path));
        session_proxy->LogForward(true);
    }

    ~LogForwardBase()
    {
        if (session_proxy && !session_closed)
        {
            try
            {
                session_proxy->LogForward(false);
            }
            catch (const DBusException&)
            {
                // Ignore errors related to disabling the log forwarding
                // here.  The session might already be closed
            }
        }
    }


private:
    OpenVPN3SessionProxy::Ptr session_proxy = {};
    bool session_closed = false;


    void ProcessSignal(const std::string sender_name,
                       const std::string obj_path,
                       const std::string interface_name,
                       const std::string signal_name,
                       GVariant *parameters) override final
    {
        if ("StatusChange" == signal_name)
        {
            StatusEvent status(parameters);
            if (status.Check(StatusMajor::CONNECTION, StatusMinor::CONN_DISCONNECTED))
            {
                session_closed = true;
            }
            StatusChangeEvent(sender_name, obj_path, interface_name, status);
        }
        else
        {
            SignalHandler(sender_name, obj_path, interface_name, signal_name,
                          parameters);
        }

    }
};
