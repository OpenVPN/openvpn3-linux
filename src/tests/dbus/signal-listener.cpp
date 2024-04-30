//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2023  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2018 - 2023  John Eismeier <john.eismeier@gmail.com>
//

/**
 * @file   signal-listener.cpp
 *
 * @brief  Simple DBusSignalSubscription test utility.  Subscribes
 *         to either all or a filtered set of signals and prints them
 *         to stdout.  If the signal type is StatusChange, ProcessChange
 *         or AttentionRequired, it will decode that information as well.
 */

#include "build-config.h"

#include <iostream>
#include <string.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>
#include <gdbuspp/signals/target.hpp>

#include "events/attention-req.hpp"
#include "events/log.hpp"
#include "events/status.hpp"
#include "common/utils.hpp"
#include "log/log-helpers.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "sessionmgr/sessionmgr-events.hpp"



class SigSubscription
{
  public:
    SigSubscription(DBus::Connection::Ptr dbuscon,
                    const std::string &bus_name,
                    const std::string &interface,
                    const std::string &object_path,
                    const std::string &signal_name)
    {
        sig_subscrptions = DBus::Signals::SubscriptionManager::Create(dbuscon);
        tgt_subscription = DBus::Signals::Target::Create(bus_name, object_path, interface);
        sig_subscrptions->Subscribe(tgt_subscription,
                                    signal_name,
                                    [=](DBus::Signals::Event::Ptr event)
                                    {
                                        try
                                        {
                                            process_signal(event);
                                        }
                                        catch (const std::exception &excp)
                                        {
                                            std::cout << "EXCEPTION: "
                                                      << excp.what()
                                                      << std::endl;
                                        }
                                    });
    }


    void process_signal(DBus::Signals::Event::Ptr event)
    {
        // Filter out NetworkManager related signals - they happen often
        // and can be a bit too disturbing
        if (event->object_path.find("/org/freedesktop/NetworkManager") != std::string::npos)
        {
            // std::cout << "NM signal ignored" << std::endl;
            return;
        }

        // Filter out certain non-OpenVPN 3 related signal interfaces
        const std::string interface_name = event->object_interface;
        if (interface_name.find("org.freedesktop.systemd1") != std::string::npos
            || interface_name.find("org.freedesktop.DBus.ObjectManager") != std::string::npos
            || interface_name.find("org.freedesktop.login1.Manager") != std::string::npos
            || interface_name.find("org.freedesktop.PolicyKit1.Authority") != std::string::npos)
        {
            return;
        }

        if (("NameOwnerChanged" == event->signal_name)
            || ("PropertiesChanged" == event->signal_name))
        {
            return;
        }

        if ("StatusChange" == event->signal_name)
        {
            Events::Status status(event->params);

            std::cout << "-- Status Change: interface=" << interface_name
                      << ", path=" << event->object_path
                      << " | " << status << std::endl;
            return;
        }
        else if ("AttentionRequired" == event->signal_name)
        {
            Events::AttentionReq ev(event->params);
            std::cout << "-- User Attention Required: "
                      << "sender=" << event->sender
                      << ", interface=" << interface_name
                      << ", path=" << event->object_path
                      << " | " << ev
                      << std::endl;
            return;
        }
        else if ("Log" == event->signal_name)
        {
            Events::Log log(event->params);
            std::cout << "-- Log: "
                      << "sender=" << event->sender
                      << ", interface=" << interface_name
                      << ", path=" << event->object_path
                      << " | " << log
                      << std::endl;
            return;
        }
        else if ("NetworkChange" == event->signal_name)
        {
            NetCfgChangeEvent ev(event->params);

            std::cout << "-- NetworkChange: "
                      << "sender=" << event->sender
                      << ", interface=" << interface_name
                      << ", path=" << event->object_path
                      << ": [" << std::to_string((std::uint8_t)ev.type)
                      << "] " << ev
                      << std::endl;
            return;
        }
        else if ("SessionManagerEvent" == event->signal_name)
        {
            SessionManager::Event ev(event->params);

            std::cout << "-- SessionManagerEvent: "
                      << "[sender=" << event->sender
                      << ", interface=" << interface_name
                      << ", path=" << event->object_path
                      << "] " << ev
                      << std::endl;
            return;
        }
        else
        {
            std::cout << "** Signal received: " << event << std::endl;
            return;
        }
    }

  private:
    DBus::Signals::SubscriptionManager::Ptr sig_subscrptions = nullptr;
    DBus::Signals::Target::Ptr tgt_subscription = nullptr;
};



int main(int argc, char **argv)
{
    std::string sig_name = (argc > 1 ? argv[1] : "");
    std::string interf = (argc > 2 ? argv[2] : "");
    std::string obj_path = (argc > 3 ? argv[3] : "");
    std::string bus_name = (argc > 4 ? argv[4] : "");

    try
    {
        auto dbus = DBus::Connection::Create(DBus::BusType::SYSTEM);
        std::cout << "Connected to D-Bus" << std::endl;

        SigSubscription subscription(dbus, bus_name, interf, obj_path, sig_name);

        std::cout << "Subscribed" << std::endl
                  << "Bus name:    " << (!bus_name.empty() ? bus_name : "(not set)") << std::endl
                  << "Interface:   " << (!interf.empty() ? interf : "(not set)") << std::endl
                  << "Object path: " << (!obj_path.empty() ? obj_path : "(not set)") << std::endl
                  << "Signal name: " << (!sig_name.empty() ? sig_name : "(not set)") << std::endl;

        auto mainloop = DBus::MainLoop::Create();
        mainloop->Run();
    }
    catch (const DBus::Exception &excp)
    {
        std::cerr << "EXCEPTION: " << excp.what() << std::endl;
        return 2;
    }
    return 0;
}
