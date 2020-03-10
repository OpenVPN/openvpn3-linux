//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019 - 2020  OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2019 - 2020  Lev Stipakov <lev@openvpn.net>
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
// FIXME: This is not properly instantiated
#include "log/core-dbus-logbase.hpp"

#include <openvpn/random/randapi.hpp>
#include <openvpn/ssl/sslchoose.hpp>
#include <openvpn/init/initprocess.hpp>
#include <openvpn/aws/awsroute.hpp>
#include <openvpn/common/jsonfile.hpp>

#include "common/utils.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "log/proxy-log.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "netcfg/proxy-netcfg.hpp"
#include "netcfg/netcfg-changeevent.hpp"


using namespace openvpn;

static const std::string OpenVPN3DBus_interf_aws = "net.openvpn.v3.aws";
static const std::string OpenVPN3DBus_path_aws = "/net/openvpn/v3/aws";


class AWSObject : public RC<thread_safe_refcount>,
                  public DBusObject,
                  public DBusSignalSubscription {
public:
    typedef  RCPtr<AWSObject> Ptr;

    AWSObject(GDBusConnection *conn, const std::string &objpath)
          : DBusObject(objpath),
            DBusSignalSubscription(conn, "", "", "", "NetworkChange"),
            netcfgmgr(conn),
            vpcRoutes {},
            log_sender(conn, LogGroup::EXTSERVICE,
                       OpenVPN3DBus_interf_aws, objpath)
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" << objpath << "'>"
                          << "    <interface name='"
                          << OpenVPN3DBus_interf_aws << "'>"
                          << "    </interface>" << "</node>";
        ParseIntrospectionXML(introspection_xml);

        // Retrieve AWS role credentials and retrieve needed VPC info
        role_name = read_role_name();
        log_sender.LogInfo("Fetching credentials from role " + role_name);

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
        THROW_DBUSEXCEPTION("ConfigManagerAlias", "set property not implemented");
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

    std::string read_role_name()
    {
        try
        {
            auto json_content = json::read_fast("/etc/openvpn3/openvpn3-aws.json");
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
            AWS::PCQuery::Ptr awspc(new AWS::PCQuery(cs, role_name));
            awspc->start([&](AWS::PCQuery::Info info) {
                ii = std::move(info);
            });
        }, nullptr, rng.get());

        return std::unique_ptr<AWS::Route::Context>(new AWS::Route::Context(ii, ii.creds, rng, nullptr, 0));
    }
};


class AWSDBus : public DBus
{
public:
    explicit AWSDBus(GDBusConnection *conn)
        : DBus(conn,
               OpenVPN3DBus_interf_aws,
               OpenVPN3DBus_path_aws,
               OpenVPN3DBus_interf_aws)
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
                                "callback_bus_acquired() - empty root_path for AWSObject, consider upgrading glib");
        aws.reset(new AWSObject(GetConnection(), root_path));
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
    AWSObject::Ptr aws;
};


int main(int argc, char **argv)
{
    // This program does not require root privileges,
    // so if used - drop those privileges
    drop_root();

    InitProcess::init();

    // Prepare the GLib GMainLoop
    GMainLoop *main_loop = g_main_loop_new(nullptr, FALSE);
    g_unix_signal_add(SIGINT, stop_handler, main_loop);
    g_unix_signal_add(SIGTERM, stop_handler, main_loop);

    DBus dbus(G_BUS_TYPE_SYSTEM);
    dbus.Connect();

    LogServiceProxy::Ptr log_service(new LogServiceProxy(dbus.GetConnection()));
    log_service->Attach(OpenVPN3DBus_interf_aws);

    AWSDBus aws(dbus.GetConnection());

    aws.Setup();

    g_main_loop_run(main_loop);

    InitProcess::uninit();

    return 0;
}
