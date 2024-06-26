//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

#include <iostream>
#include <iomanip>
#include <gdbuspp/service.hpp>
#include <gdbuspp/proxy/utils.hpp>
#include <sstream>
#include <exception>
#include <set>
#include <glib-unix.h>

// Needs to be included before openvpn3-core library
// It provides needed logging facility used by Core lib.
#include "log/core-dbus-logger.hpp"

#include <openvpn/random/randapi.hpp>
#include <openvpn/ssl/sslchoose.hpp>
#include <openvpn/init/initprocess.hpp>
#include <openvpn/aws/awsroute.hpp>
#include <openvpn/common/jsonfile.hpp>

#include "common/cmdargparser.hpp"
#include "common/utils.hpp"
#include "log/logwriter.hpp"
#include "log/logwriters/implementations.hpp"
#include "log/ansicolours.hpp"
#include "log/proxy-log.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "netcfg/proxy-netcfg-mgr.hpp"

using namespace openvpn;

#define OPENVPN3_AWS_CONFIG "/etc/openvpn3/openvpn3-aws.json"
#define OPENVPN3_AWS_CERTS "/etc/openvpn3/awscerts"

/**
 * Helper class to tackle log signals sent by the AWSObject
 *
 * This mostly just wraps the LogSender class and predefines LogGroup to always
 * be EXTSERVICE.
 */
class AWSLog : public LogSender
{
  public:
    using Ptr = std::shared_ptr<AWSLog>;

    AWSLog(DBus::Connection::Ptr conn,
           const std::string &object_path,
           LogWriter::Ptr logwr)
        : LogSender(conn, LogGroup::EXTSERVICE, object_path, Constants::GenInterface("aws"), false, logwr.get())
    {
        auto srvqry = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
        if (!srvqry->CheckServiceAvail(Constants::GenServiceName("log")))
        {
            throw DBus::Object::Exception("Could not connect to log service");
        }

        auto creds = DBus::Credentials::Query::Create(conn);
        AddTarget(creds->GetUniqueBusName(Constants::GenServiceName("log")));
    }
};

class AWSObject : public DBus::Object::Base
{
  public:
    AWSObject(DBus::Connection::Ptr dbuscon,
              const std::string &config_file,
              LogWriter::Ptr logwr,
              unsigned int log_level)
        : DBus::Object::Base(Constants::GenPath("aws"),
                             Constants::GenInterface("aws")),
          netcfg_mgr(NetCfgProxy::Manager::Create(dbuscon)),
          subscr_mgr(DBus::Signals::SubscriptionManager::Create(dbuscon)),
          log(std::make_shared<AWSLog>(dbuscon, GetPath(), logwr))
    {
        log->SetLogLevel(log_level);
        CoreLog::Connect(log);

        RegisterSignals(log);

        auto dbussrvq = DBus::Proxy::Utils::DBusServiceQuery::Create(dbuscon);
        signals_target = DBus::Signals::Target::Create(dbussrvq->GetNameOwner(Constants::GenServiceName("netcfg")),
                                                       "",
                                                       Constants::GenServiceName("netcfg"));
        // Retrieve AWS role credentials and retrieve needed VPC info
        role_name = read_role_name(config_file);
        if (role_name.empty())
        {
            throw DBus::Object::Exception("No role defined");
        }

        log->LogInfo("Fetching credentials from role '" + role_name + "'");

        auto route_context = prepare_route_context(role_name);
        AWS::Route::Info route_info{*route_context};
        route_table_id = route_info.route_table_id;
        network_interface_id = route_info.network_interface_id;
        AWS::Route::set_source_dest_check(*route_context,
                                          network_interface_id,
                                          false);

        log->LogInfo("Running on instance " + route_context->instance_id() + ", route table " + route_table_id);

        subscr_mgr->Subscribe(signals_target, "NetworkChange", [=](DBus::Signals::Event::Ptr &event)
                              {
                                  process_network_change(event);
                              });

        netcfg_mgr->NotificationSubscribe(NetCfgChangeType::ROUTE_ADDED | NetCfgChangeType::ROUTE_REMOVED);

        // We will act upon route changes caused by VPN sessions,
        // notifications which are sent by the net.openvpn.v3.netcfg service
    }

    ~AWSObject() override
    {
        // We are shutting down, stop notification subscriptions
        // before we start cleaning up.
        netcfg_mgr->NotificationUnsubscribe();

        // Retrieve AWS credentials and remove routes we are responsible
        // for from VPC
        auto route_context = prepare_route_context(role_name);

        for (auto route : vpc_routes)
        {
            try
            {
                AWS::Route::delete_route(*route_context,
                                         route_table_id,
                                         route.first,
                                         route.second);

                log->LogInfo("Removed route " + route.first);
            }
            catch (const std::exception &ex)
            {
                log->LogError("Error removing route: " + std::string(ex.what()));
            }
        }

        vpc_routes.clear();
    }

    const bool Authorize(const DBus::Authz::Request::Ptr request) override
    {
        return true;
    }

    /**
     *  Called each time the subscribed signal has a match on the D-Bus
     */
    void process_network_change(DBus::Signals::Event::Ptr &event)
    {
        if (event->signal_name != "NetworkChange")
        {
            return;
        }

        // Parse the network change event, only consider route changes
        NetCfgChangeEvent ev(event->params);
        if ((ev.type != NetCfgChangeType::ROUTE_ADDED)
            && (ev.type != NetCfgChangeType::ROUTE_REMOVED))
        {
            return;
        }

        try
        {
            const std::string cidr = ev.details["subnet"] + "/" + ev.details["prefix"];
            const bool ipv6 = ev.details["ip_version"] == "6";
            auto route_context = prepare_route_context(role_name);

            if (ev.type == NetCfgChangeType::ROUTE_ADDED)
            {
                AWS::Route::replace_create_route(*route_context,
                                                 route_table_id,
                                                 cidr,
                                                 AWS::Route::RouteTargetType::INSTANCE_ID,
                                                 route_context->instance_id(),
                                                 ipv6);

                vpc_routes.emplace(cidr, ipv6);

                log->LogInfo("Added route " + cidr);
            }
            else
            {
                AWS::Route::delete_route(*route_context,
                                         route_table_id,
                                         cidr,
                                         ipv6);

                auto it = vpc_routes.find(VpcRoute(cidr, ipv6));
                if (it != vpc_routes.end())
                {
                    vpc_routes.erase(it);
                }

                log->LogInfo("Removed route " + cidr);
            }
        }
        catch (const std::exception &ex)
        {
            log->LogError("Error updating VPC routing: " + std::string(ex.what()));
        }
    }

  private:
    NetCfgProxy::Manager::Ptr netcfg_mgr;
    DBus::Signals::SubscriptionManager::Ptr subscr_mgr;
    DBus::Signals::Target::Ptr signals_target;

    std::string role_name;
    std::string network_interface_id;
    std::string route_table_id;
    typedef std::pair<std::string, bool> VpcRoute;
    std::set<VpcRoute> vpc_routes;

    AWSLog::Ptr log;

    std::string read_role_name(const std::string &config_file)
    {
        try
        {
            auto json_content = json::read_fast(config_file);
            return json::get_string(json_content, "role");
        }
        catch (const std::exception &ex)
        {
            log->LogError("Error reading role name: " + std::string(ex.what()));
            return "";
        }
    }

    std::unique_ptr<AWS::Route::Context> prepare_route_context(const std::string &role_name)
    {
        StrongRandomAPI::Ptr rng(new SSLLib::RandomAPI());
        AWS::PCQuery::Info ii;

        WS::ClientSet::run_synchronous([&](WS::ClientSet::Ptr cs)
                                       {
                                           AWS::PCQuery::Ptr awspc(new AWS::PCQuery(cs, role_name, OPENVPN3_AWS_CERTS));
                                           awspc->start([&](AWS::PCQuery::Info info)
                                                        {
                                                            if (info.is_error())
                                                            {
                                                                throw DBus::Object::Exception("Error preparing route context: " + info.error);
                                                            }
                                                            ii = std::move(info);
                                                        });
                                       },
                                       nullptr,
                                       rng.get());

        return std::unique_ptr<AWS::Route::Context>(new AWS::Route::Context(ii, ii.creds, rng, nullptr, 0));
    }
};


class AWSDBus : public DBus::Service
{
  public:
    AWSDBus(DBus::Connection::Ptr dbuscon, const std::string &config_file, LogWriter::Ptr logwr, unsigned int log_level)
        : DBus::Service(dbuscon, Constants::GenServiceName("aws")),
          config_file{config_file},
          logwr{logwr},
          log_level{log_level}
    {
        try
        {
            logsrvprx = LogServiceProxy::AttachInterface(dbuscon, Constants::GenInterface("aws"));
        }
        catch (const DBus::Exception &excp)
        {
            logwr->Write(LogGroup::EXTSERVICE,
                         LogCategory::CRIT,
                         excp.GetRawError());
        }
    }

    ~AWSDBus() noexcept
    {
        if (logsrvprx)
        {
            logsrvprx->Detach(Constants::GenInterface("aws"));
        }
    }

    void BusNameAcquired(const std::string &busname) override
    {
        CreateServiceHandler<AWSObject>(GetConnection(),
                                        config_file,
                                        logwr,
                                        log_level);
    }

    void BusNameLost(const std::string &busname) override
    {
    }

  private:
    std::string config_file;
    LogWriter::Ptr logwr;
    unsigned int log_level;
    LogServiceProxy::Ptr logsrvprx = nullptr;
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
    std::ostream *logfile = nullptr;
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

    unsigned int log_level = 3;
    if (args->Present("log-level"))
    {
        log_level = std::atoi(args->GetValue("log-level", 0).c_str());
    }

    std::string config_file{OPENVPN3_AWS_CONFIG};
    if (args->Present("config"))
    {
        config_file = args->GetValue("config", 0);
    }

    // Initialize Core library
    InitProcess::Init init;

    auto dbuscon = DBus::Connection::Create(DBus::BusType::SYSTEM);

    try
    {
        // Instantiate and prepare the D-Bus service provided by AWSDBus
        auto aws_srv = DBus::Service::Create<AWSDBus>(dbuscon, config_file, logwr, log_level);

        // Start the service
        aws_srv->Run();
    }
    catch (const DBus::Exception &exc)
    {
        throw CommandException("AWS", exc.what());
    }

    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand cmd(argv[0], "OpenVPN 3 AWS VPC integration service", aws_main);
    cmd.AddVersionOption();
    cmd.AddOption("config",
                  'c',
                  "FILE",
                  true,
                  "AWS VPC configuration file (default: " OPENVPN3_AWS_CONFIG ")");
    cmd.AddOption("log-file",
                  "FILE",
                  true,
                  "Write log data to FILE.  Use 'stdout:' for console logging.");
    cmd.AddOption("log-level",
                  "LOG-LEVEL",
                  true,
                  "Log verbosity level (valid values 0-6, default 3)");
    cmd.AddOption("colour",
                  0,
                  "Make the log lines colourful");
    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const LogServiceProxyException &excp)
    {
        std::cout << "** ERROR ** " << excp.what() << std::endl;
        std::cout << "            " << excp.debug_details() << std::endl;
        return 2;
    }
    catch (CommandException &excp)
    {
        std::cout << cmd.GetCommand() << ": " << excp.what() << std::endl;
        return 2;
    }
}
