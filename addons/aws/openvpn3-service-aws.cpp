//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019 - 2022  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2019 - 2022  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2020 - 2022  David Sommerseth <davids@openvpn.net>
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

#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>
#include <set>
#include <glib-unix.h>

// Needs to be included before openvpn3-core library
// It provides needed logging facility used by Core lib.
#include "log/core-dbus-logbase.hpp"

#include <openvpn/random/randapi.hpp>
#include <openvpn/ssl/sslchoose.hpp>
#include <openvpn/init/initprocess.hpp>
#include <openvpn/aws/awsroute.hpp>
#include <openvpn/common/jsonfile.hpp>

#include "common/utils.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "log/logwriter.hpp"
#include "log/ansicolours.hpp"
#include "log/proxy-log.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "netcfg/proxy-netcfg.hpp"
#include "netcfg/netcfg-changeevent.hpp"


using namespace openvpn;

#define OPENVPN3_AWS_CONFIG "/etc/openvpn3/openvpn3-aws.json"
#define OPENVPN3_AWS_CERTS "/etc/openvpn3/awscerts"
static const std::string OpenVPN3DBus_interf_aws = "net.openvpn.v3.aws";
static const std::string OpenVPN3DBus_path_aws = "/net/openvpn/v3/aws";


class AWSObject : public RC<thread_safe_refcount>,
                  public DBusObject,
                  public DBusSignalSubscription {
public:
    typedef  RCPtr<AWSObject> Ptr;

    AWSObject(GDBusConnection* conn, const std::string& objpath,
              const std::string& config_file,
              LogWriter* logwr, unsigned int log_level, bool signal_broadcast)
          : DBusObject(objpath),
            DBusSignalSubscription(conn, "", "", "", "NetworkChange"),
            netcfgmgr(conn),
            vpcRoutes {},
            log_sender(conn, LogGroup::EXTSERVICE,
                       OpenVPN3DBus_interf_aws, objpath, logwr)
    {
        log_sender.SetLogLevel(log_level);
        if (!signal_broadcast)
        {
            DBusConnectionCreds credsprx(conn);
            log_sender.AddTargetBusName(credsprx.GetUniqueBusID(OpenVPN3DBus_name_log));

        }
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='"
                          << OpenVPN3DBus_interf_aws << "'>"
                          << "    </interface>" << "</node>";
        ParseIntrospectionXML(introspection_xml);

        // Retrieve AWS role credentials and retrieve needed VPC info
        role_name = read_role_name(config_file);
        if (role_name.empty())
        {
            THROW_DBUSEXCEPTION("AWSObject", "No role defined");
        }
        log_sender.LogInfo("Fetching credentials from role '"
                            + role_name + "'");

        auto route_context = prepare_route_context(role_name);
        AWS::Route::Info route_info { *route_context };
        route_table_id = route_info.route_table_id;
        network_interface_id = route_info.network_interface_id;
        AWS::Route::set_source_dest_check(*route_context,
                                          network_interface_id, false);

        log_sender.LogInfo("Running on instance " + route_context->instance_id()
                           + ", route table " + route_table_id);

        // We will act upon route changes caused by VPN sessions,
        // notifications which are sent by the net.openvpn.v3.netcfg service
        netcfgmgr.NotificationSubscribe(NetCfgChangeType::ROUTE_ADDED
                                        | NetCfgChangeType::ROUTE_REMOVED);
    }

    ~AWSObject() override
    {
        // We are shutting down, stop notification subscriptions
        // before we start cleaning up.
        netcfgmgr.NotificationUnsubscribe();

        // Retrieve AWS credentials and remove routes we are responsible
        // for from VPC
        auto route_context = prepare_route_context(role_name);

        for (auto vpcRoute : vpcRoutes)
        {
            try
            {
                AWS::Route::delete_route(*route_context,
                                         route_table_id,
                                         vpcRoute.first,
                                         vpcRoute.second);

                log_sender.LogInfo("Removed route " + vpcRoute.first);
            }
            catch (const std::exception& ex)
            {
                log_sender.LogError("Error removing route: " + std::string(ex.what()));
            }
        }

        vpcRoutes.clear();
    }

    /**
     *  Called each time the subscribed signal has a match on the D-Bus
     */
    void callback_signal_handler(GDBusConnection *connection,
                                 const std::string sender_name,
                                 const std::string object_path,
                                 const std::string interface_name,
                                 const std::string signal_name,
                                 GVariant *parameters) override
    {
        if (signal_name != "NetworkChange")
        {
            return;
        }

        // Parse the network change event, only consider route changes
        NetCfgChangeEvent ev(parameters);
        if ((ev.type != NetCfgChangeType::ROUTE_ADDED) &&
            (ev.type != NetCfgChangeType::ROUTE_REMOVED))
        {
            return;
        }

        try
        {
            const std::string cidr = ev.details["subnet"] + "/" + ev.details["prefix"];
            const bool ipv6 =  ev.details["ip_version"] == "6";
            auto route_context = prepare_route_context(role_name);

            if (ev.type == NetCfgChangeType::ROUTE_ADDED)
            {
                AWS::Route::replace_create_route(*route_context,
                                                 route_table_id,
                                                 cidr,
                                                 AWS::Route::RouteTargetType::INSTANCE_ID,
                                                 route_context->instance_id(),
                                                 ipv6);

                vpcRoutes.emplace(cidr, ipv6);

                log_sender.LogInfo("Added route " + cidr);
            }
            else
            {
                AWS::Route::delete_route(*route_context,
                                         route_table_id,
                                         cidr,
                                         ipv6);

                auto it = vpcRoutes.find(VpcRoute(cidr, ipv6));
                if (it != vpcRoutes.end())
                {
                    vpcRoutes.erase(it);
                }

                log_sender.LogInfo("Removed route " + cidr);
            }
        }
        catch (const std::exception& ex)
        {
            log_sender.LogError("Error updating VPC routing: " + std::string(ex.what()));
        }
    }


    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error) override
    {
        GVariant *ret = nullptr;

        ret = nullptr;
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Unknown property");
        return ret;
    }


    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error) override
    {
        THROW_DBUSEXCEPTION("AWSDBus", "set property not implemented");
    }


    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc) override
    {
    }


private:
    NetCfgProxy::Manager netcfgmgr;
    std::string role_name;
    std::string network_interface_id;
    std::string route_table_id;
    typedef std::pair<std::string, bool> VpcRoute;
    std::set<VpcRoute> vpcRoutes;
    LogSender log_sender;

    std::string read_role_name(const std::string& config_file)
    {
        try
        {
            auto json_content = json::read_fast(config_file);
            return json::get_string(json_content, "role");
        }
        catch (const std::exception & ex)
        {
            log_sender.LogError("Error reading role name: " + std::string(ex.what()));
            return "";
        }
    }

    std::unique_ptr<AWS::Route::Context> prepare_route_context(const std::string& role_name)
    {
        RandomAPI::Ptr rng(new SSLLib::RandomAPI(false));
        AWS::PCQuery::Info ii;

        WS::ClientSet::run_synchronous([&](WS::ClientSet::Ptr cs) {
            AWS::PCQuery::Ptr awspc(new AWS::PCQuery(cs, role_name, OPENVPN3_AWS_CERTS));
            awspc->start([&](AWS::PCQuery::Info info) {
                if (info.is_error())
                {
                    THROW_DBUSEXCEPTION("AWSObject", "Error preparing route context: " + info.error);
                }
                ii = std::move(info);
            });
        }, nullptr, rng.get());

        return std::unique_ptr<AWS::Route::Context>(new AWS::Route::Context(ii, ii.creds, rng, nullptr, 0));
    }
};


class AWSDBus : public DBus
{
public:
    explicit AWSDBus(GDBusConnection* conn, const std::string& config_file,
                     LogWriter* logwr, unsigned int log_level, bool sig_brdc)
        : DBus(conn,
               OpenVPN3DBus_interf_aws,
               OpenVPN3DBus_path_aws,
               OpenVPN3DBus_interf_aws),
           config_file{config_file},
           logwr{logwr},
           log_level{log_level},
           signal_broadcast{sig_brdc}
    {
    }

    /**
     *  This callback is called when the service was successfully registered
     *  on the D-Bus.
     */
    void callback_bus_acquired() override
    {
        auto root_path = GetRootPath();
        if (root_path.empty())
            THROW_DBUSEXCEPTION("AWSDBus",
                                "callback_bus_acquired() - "
                                "empty root_path for AWSObject, consider upgrading glib");
        aws.reset(new AWSObject(GetConnection(), root_path, config_file,
                                logwr, log_level, signal_broadcast));
        aws->RegisterObject(GetConnection());
    }


    /**
     *  This is called each time the well-known bus name is successfully
     *  acquired on the D-Bus.
     *
     *  This is not used, as the preparations already happens in
     *  callback_bus_acquired()
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the acquired bus name
     */
    void callback_name_acquired(GDBusConnection *conn, std::string busname) override
    {
    }

    /**
     *  This is called each time the well-known bus name is removed from the
     *  D-Bus.  In our case, we just throw an exception and starts shutting
     *  down.
     *
     * @param conn     Connection where this event happened
     * @param busname  A string of the lost bus name
     */
    void callback_name_lost(GDBusConnection *conn, std::string busname) override
    {
        THROW_DBUSEXCEPTION("AWSDBus",
                            "openvpn3-service-aws could not register '"
                            + busname + "' on the D-Bus");
    }

private:
    std::string config_file;
    LogWriter* logwr;
    unsigned int log_level;
    bool signal_broadcast;
    AWSObject::Ptr aws;
};


int aws_main(ParsedArgs::Ptr args)
{
    // This program does not require root privileges,
    // so if used - drop those privileges
    drop_root();

    //
    // Open a log destination, if requested
    //
    // This is opened before dropping privileges, to more easily tackle
    // scenarios where logging goes to a file in /var/log or other
    // directories where only root has access
    //
    std::ofstream logfs;
    std::ostream  *logfile = nullptr;
    LogWriter::Ptr logwr = nullptr;
    ColourEngine::Ptr colourengine = nullptr;

    if (args->Present("log-file"))
    {
        std::string fname = args->GetValue("log-file", 0);

        if ("stdout:" != fname)
        {
            logfs.open(fname.c_str(), std::ios_base::app);
            logfile = &logfs;
        }
        else
        {
            logfile = &std::cout;
        }

        if (args->Present("colour"))
        {
            colourengine.reset(new ANSIColours());
            logwr.reset(new ColourStreamWriter(*logfile,
                                               colourengine.get()));
        }
        else
        {
            logwr.reset(new StreamLogWriter(*logfile));
        }
    }

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

    // Initialize logging in the OpenVPN 3 Core library
    openvpn::CoreDBusLogBase corelog(dbus.GetConnection(),
                                     OpenVPN3DBus_interf_aws + ".core",
                                     LogGroup::EXTSERVICE,
                                     logwr.get());
    corelog.SetLogLevel(log_level);


    bool signal_broadcast = args->Present("signal-broadcast");
    LogServiceProxy::Ptr logsrvprx = nullptr;
    if (!signal_broadcast)
    {
        logsrvprx = LogServiceProxy::AttachInterface(dbus.GetConnection(),
                                                     OpenVPN3DBus_interf_aws);
        logsrvprx->Attach(OpenVPN3DBus_interf_aws + ".core");
    }

    std::string config_file{OPENVPN3_AWS_CONFIG};
    if (args->Present("config"))
    {
        config_file = args->GetValue("config", 0);
    }

    // Initialize Core library
    InitProcess::Init init;

    // Prepare the GLib GMainLoop
    GMainLoop *main_loop = g_main_loop_new(nullptr, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    try
    {
        // Prepare the net.openvpn.v3.aws D-Bus service
        AWSDBus aws(dbus.GetConnection(), config_file,
                    logwr.get(), log_level, signal_broadcast);
        aws.Setup();

        // Start the main program loop
        g_main_loop_run(main_loop);

        // Clean up before exit
        g_main_loop_unref(main_loop);
        if (logsrvprx)
        {
            logsrvprx->Detach(OpenVPN3DBus_interf_aws + ".core");
            logsrvprx->Detach(OpenVPN3DBus_interf_aws);
        }
    }
    catch (const DBusException& exc)
    {
        throw CommandException("AWS", exc.what());
    }

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand cmd(argv[0], "OpenVPN 3 AWS VPC integration service",
                             aws_main);
    cmd.AddVersionOption();
    cmd.AddOption("config", 'c', "FILE", true,
                  "AWS VPC configuration file (default: " OPENVPN3_AWS_CONFIG ")");
    cmd.AddOption("log-file", "FILE" , true,
                  "Write log data to FILE.  Use 'stdout:' for console logging.");
    cmd.AddOption("log-level", "LOG-LEVEL", true,
                  "Log verbosity level (valid values 0-6, default 3)");
    cmd.AddOption("colour", 0,
                        "Make the log lines colourful");
    cmd.AddOption("signal-broadcast", 0,
                  "Broadcast all D-Bus signals from openvpn3-service-aws");
    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException& excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        return 2;
    }
    catch (CommandException& excp)
    {
        std::cout << cmd.GetCommand() << ": " << excp.what() << std::endl;
        return 2;
    }

}
