//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   log-listener.cpp
 *
 * @brief  Subscribes to any log signals sent from either the session manager
 *         or a backend service and prints them to stdout.  This uses a
 *         simpler and more low-level approach via LogSubscription.
 */

#include <iostream>
#include <string.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>

#include "build-config.h"
#include "dbus/constants.hpp"
#include "dbus/signals/log.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"


class LogHandler
{
  public:
    LogHandler(DBus::Connection::Ptr conn,
               const std::string &ltag,
               const DBus::Object::Path &object_path,
               const std::string &interface)
        : tag(ltag)
    {
        subscr_target = DBus::Signals::Target::Create("",
                                                      object_path,
                                                      interface);
        subscrmgr = DBus::Signals::SubscriptionManager::Create(conn);
        logrecv = Signals::ReceiveLog::Create(
            subscrmgr,
            subscr_target,
            [=](Events::Log logevent)
            {
              std::cout << "[" << tag << "] " << logevent << std::endl;
            });
    }

  private:
    const std::string tag;
    Signals::ReceiveLog::Ptr logrecv = nullptr;
    DBus::Signals::SubscriptionManager::Ptr subscrmgr = nullptr;
    DBus::Signals::Target::Ptr subscr_target = nullptr;
};


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <session path>" << std::endl;
        return 1;
    }

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto sessmgr = SessionManager::Proxy::Manager::Create(dbuscon);
    auto session = sessmgr->Retrieve(argv[1]);

    LogHandler lh_sess(dbuscon, "Session", "", Constants::GenInterface("sessions"));
    LogHandler lh_be(dbuscon, "Backend", "", Constants::GenInterface("backends"));
    session->LogForward(true);

    std::cout << "Subscribed" << std::endl;

    auto mainloop = DBus::MainLoop::Create();
    mainloop->Run();

    std::cout << "Unsubscribed" << std::endl;

    session->LogForward(false);

    return 0;
}
