//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log/log-attach.hpp
 *
 * @brief  Helper class to prepare setting up the EventLogger object
 */

#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "sessionmgr/sessionmgr-events.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#include "event-logger.hpp"


namespace ovpn3cli::log {

/**
 *  Helper class to attach to log events from sessions
 *
 *  This class implements logic to also wait until the session manager
 *  signals a newly created session when the log attach is tied to a
 *  configuration profile name.  Once the session is found, the SessionLogger
 *  class takes over the log event handling itself.  The LogAttach object
 *  will also stop the logging once the session manager signals the session
 *  has been destroyed.
 *
 */
class LogAttach
{
  public:
    using Ptr = std::shared_ptr<LogAttach>;

    [[nodiscard]] static Ptr Create(DBus::MainLoop::Ptr mainloop,
                                    DBus::Connection::Ptr dbuscon);
    ~LogAttach() noexcept;

    void AttachByPath(const DBus::Object::Path &path);

    void AttachByConfig(const std::string &config);

    void AttachByInterface(const std::string &interf);

    void SetLogLevel(const unsigned int loglvl);


  private:
    DBus::MainLoop::Ptr mainloop = nullptr;
    DBus::Connection::Ptr dbuscon = nullptr;
    DBus::Signals::SubscriptionManager::Ptr signal_subscr = nullptr;
    SessionManager::Proxy::Manager::Ptr manager = nullptr;
    SessionManager::Proxy::Session::Ptr session_proxy = nullptr;
    EventLogger::Ptr eventlogger = nullptr;
    DBus::Object::Path session_path{};
    std::string config_name{};
    std::string tun_interf{};
    uint32_t log_level = 0;
    bool wait_notification = false;


    LogAttach(DBus::MainLoop::Ptr main_loop,
              DBus::Connection::Ptr dbusc);

    void sessionmgr_event(const SessionManager::Event &event);

    void lookup_config_name(const std::string &cfgname);

    void lookup_interface(const std::string &interf);


    /**
     *  Create a new EventLogger object for processing log and status event
     *  changes for a running session.
     *
     *  The input path is used to ensure a SESS_CREATED SessionManager::Event
     *  is tied to the VPN session we expect.
     *
     * @param path  DBus::Object::Path to the VPN session to attach to.
     */
    void setup_session_logger(const DBus::Object::Path &path);
};


}
