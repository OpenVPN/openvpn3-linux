//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
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
 * @file   log.hpp
 *
 * @brief  Commands related to receive log entries from various sessions
 */

#include "dbus/core.hpp"
#include "log/dbus-log.hpp"

using namespace openvpn;


/**
 *  Simple class to handle Log signal events
 */
class Logger : public LogConsumer,
               public RC<thread_unsafe_refcount>
{
public:
    typedef RCPtr<Logger> Ptr;

    /**
     *  When instantiating a new Logger object, it will subscribe to
     *  Log signals appearing on a specific interface and D-Bus object path.
     *  If these are empty, it will listen to any Log signals
     *
     * @param dbscon       D-Bus connection to use for subscribing
     * @param interf       std::string containing the interface to subscribe to
     * @param object_path  std::string with the D-Bus object path to subscribe to
     */
    Logger(GDBusConnection * dbscon, std::string interf,
           std::string object_path)
        : LogConsumer(dbscon, interf, object_path)
    {
    }


    /**
     *  This method is called on each Log signal event.
     *
     * @param sender       std::string with the sender of the Log event sender
     * @param interface    std::string with the interface the Log signal origins from
     * @param object_path  std::string with the D-Bus object path of the Log event
     * @param group        LogGroup indicator of the Log event
     * @param catg         LogCategory indicator of the Log event
     * @param msg          std::string carrying the Log message
     */
    void ConsumeLogEvent(const std::string sender,
                         const std::string interface,
                         const std::string object_path,
                         const LogGroup group, const LogCategory catg,
                         const std::string msg)
    {
        std::cout << GetTimestamp()
                  << LogPrefix(group, catg) << msg
                  << std::endl;
    }
};


/**
 *  Simple log command which can retrieve Log events happening on a specific
 *  session path or coming from the configuration manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_log_listen(ParsedArgs args)
{
    if (!args.Present("session-path") && !args.Present("config-events"))
    {
        throw CommandException("log",
                               "Either --session-path or --config-events must be provided");
    }

    Logger::Ptr session_log;
    Logger::Ptr config_log;
    bool log_flag_reset = false;
    std::string session_path = "";
    DBus dbuscon(G_BUS_TYPE_SYSTEM);
    dbuscon.Connect();

    if (args.Present("session-path"))
    {
        session_path = args.GetValue("session-path", 0);

        // Check if the session manager will forward log events for this
        // session.  If not, we must enable it - and if we do this, we
        // track that we modified this setting.
        OpenVPN3SessionProxy sesprx(G_BUS_TYPE_SYSTEM, session_path);
        sesprx.Ping();

        if (!sesprx.GetBoolProperty("receive_log_events"))
        {
            sesprx.SetProperty("receive_log_events", true);
            log_flag_reset = true;
        }
        // Setup a Logger object for the provided session path
        session_log.reset(new Logger(dbuscon.GetConnection(),
                                     OpenVPN3DBus_interf_sessions,
                                     session_path));
    }

    if (args.Present("config-events"))
    {
        // Set up a generic logger for configuration management related
        // Log events.  We don't provide an object path, as all events
        // occur with the same object path.  Filtering would not make any
        // difference on the end result.
        config_log.reset(new Logger(dbuscon.GetConnection(),
                                    OpenVPN3DBus_interf_configuration, ""));
    }

    // Prepare the main loop which will listen for Log events and process them
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    // Start the main loop.  This will exit on SIGINT or SIGTERM signals only
    g_main_loop_run(main_loop);

    // If we enabled the receive_log_events property in a session object,
    // disable it again when we exit.
    if (!session_path.empty() && log_flag_reset)
    {
        OpenVPN3SessionProxy sesprx(G_BUS_TYPE_SYSTEM, session_path);
        sesprx.SetProperty("receive_log_events", false);
    }

    // Clean-up and shut down.
    g_main_loop_unref(main_loop);

    return 0;
}


/**
 *  Declare all the supported commands and their options and arguments.
 *
 *  This function should only be called once by the main openvpn3 program,
 *  which sends a reference to the Commands argument parser which is used
 *  for this registration process
 *
 * @param ovpn3  Commands object where to register all the commands, options
 *               and arguments.
 */
void RegisterCommands_log(Commands& ovpn3)
{
    //
    //  session-start command
    //
    auto cmd = ovpn3.AddCommand("log",
                                "Receive log events as they occur",
                                cmd_log_listen);
    cmd->AddOption("session-path", "SESSION-PATH", true,
                   "Receive log events for a specific session",
                   arghelper_session_paths);
    cmd->AddOption("config-events",
                   "Receive log events issued by the configuration manager");
}
