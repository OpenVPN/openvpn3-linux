//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2022  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2022  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logfwd-listener.cpp
 *
 * @brief  Attaches to Log signals on a running session.  This requires
 *         the net.openvpn.v3.log service to support ProxyLogEvents
 *
 */

#include <iostream>
#include <string.h>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/mainloop.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <gdbuspp/signals/subscriptionmgr.hpp>
#include <gdbuspp/signals/target.hpp>

#include "common/utils.hpp"
#include "dbus/constants.hpp"
#include "dbus/signals/log.hpp"
#include "dbus/signals/statuschange.hpp"
#include "log/ansicolours.hpp"
#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"

#include "sessionmgr/proxy-sessionmgr.hpp"

class EventLogger
{
  public:
    EventLogger(DBus::MainLoop::Ptr mainl,
                LogWriter::Ptr logdst,
                DBus::Connection::Ptr conn,
                const DBus::Object::Path &session_path);

  private:
    DBus::MainLoop::Ptr mainloop = nullptr;
    LogWriter::Ptr logdest = nullptr;
    DBus::Signals::SubscriptionManager::Ptr submgr = nullptr;
    DBus::Signals::Target::Ptr signal_sender = nullptr;
    Signals::ReceiveLog::Ptr sighandler_log = nullptr;
    Signals::ReceiveStatusChange::Ptr sighandler_statuschg = nullptr;
};


EventLogger::EventLogger(DBus::MainLoop::Ptr mainl,
                         LogWriter::Ptr logdst,
                         DBus::Connection::Ptr conn,
                         const DBus::Object::Path &session_path)
    : mainloop(mainl), logdest(logdst)
{
    auto prxqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
    auto log_busname = prxqry->GetNameOwner(Constants::GenServiceName("log"));

    submgr = DBus::Signals::SubscriptionManager::Create(conn);
    signal_sender = DBus::Signals::Target::Create(log_busname, session_path, "");

    sighandler_log = Signals::ReceiveLog::Create(
        submgr,
        signal_sender,
        [&](const Events::Log logev)
        {
            Events::Log event(logev);
            event.message = logev.str(26);
            logdest->Write(event);
        });

    sighandler_statuschg = Signals::ReceiveStatusChange::Create(
        submgr,
        signal_sender,
        [&](const std::string &sender, const DBus::Object::Path &path, const std::string &interface, Events::Status stchgev)
        {
            logdest->WriteStatus(stchgev);
            if (StatusMajor::CONNECTION == stchgev.major
                && (StatusMinor::CONN_DONE == stchgev.minor))
            {
                logdest->Write("*** Stopping mainloop");
                mainloop->Stop();
            }
        });
}


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <--config|--session-path> <config name|session path>" << std::endl;
        return 1;
    }

    std::string mode{argv[1]};
    std::string session_path{};

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);
    auto mgr = SessionManager::Proxy::Manager::Create(dbuscon);
    if ("--config" == mode)
    {
        auto sp = mgr->LookupConfigName(argv[2]);
        if (sp.size() > 1)
        {
            std::cerr << "More than one session was found with '" << argv[2]
                      << "' as config name" << std::endl;
            return 2;
        }
        else if (sp.size() == 0)
        {
            std::cerr << "No sessions found with '" << argv[2]
                      << "' as config name" << std::endl;
            return 2;
        }
        session_path = sp[0];
    }
    else if ("--session-path" == mode)
    {
        session_path = DBus::Object::Path(argv[2]);
    }
    else
    {
        std::cerr << "** ERROR **  Need --config or --session-path" << std::endl;
        return 2;
    }

    std::cout << "Attaching to session path: " << session_path << std::endl;
    auto session = mgr->Retrieve(session_path);

    std::ostream logdest(std::cout.rdbuf());
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;
    if (is_colour_terminal())
    {
        colourengine.reset(new ANSIColours());
        logwr.reset(new ColourStreamWriter(logdest, colourengine.get()));
    }
    else
    {
        logwr.reset(new StreamLogWriter(logdest));
    }

    try
    {
        std::cout << "D-Bus bus name: " << dbuscon->GetUniqueBusName() << std::endl;
        std::cout << "Log level: " << std::to_string(session->GetLogVerbosity()) << std::endl;


        auto mainloop = DBus::MainLoop::Create();
        EventLogger logger(mainloop, logwr, dbuscon, session_path);

        session->LogForward(true);
        mainloop->Run();
        session->LogForward(false);
    }
    catch (const DBus::Exception &e)
    {
        session->LogForward(false);
        std::cerr << "** ERROR ** " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
