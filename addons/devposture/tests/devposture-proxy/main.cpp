//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2024-  Răzvan Cojocaru <razvan.cojocaru@openvpn.com>
//

#include <ctime>
#include <iostream>
#include <json/json.h>
#include "src/client/proxy-devposture.hpp"
#include "src/common/cmdargparser.hpp"
#include "src/dbus/path.hpp"
#include "src/common/utils.hpp"


int devposture_proxy(ParsedArgs::Ptr args)
{
    std::string mode;
    try
    {
        args->CheckExclusiveOptions({{"list-modules", "enterprise-profile"},
                                     {"list-modules", "protocol"},
                                     {"protocol", "enterprise-profile"}});
        mode = args->Present({"list-modules", "enterprise-profile", "protocol"});
    }
    catch (const OptionNotFound &)
    {
        throw CommandException(args->GetArgv0(),
                               "Missing arguments, see --help");
    }
    catch (const ExclusiveOptionError &excp)
    {
        throw CommandException(args->GetArgv0(), excp.what());
    }

    drop_root();
    try
    {
        auto connection = DBus::Connection::Create(DBus::BusType::SYSTEM);
        auto proxy = DevPosture::Proxy::Handler::Create(connection);

        // List just the registered modules
        if ("list-modules" == mode)
        {
            std::cout << "*** Registered modules:\n";

            for (const auto &m : proxy->GetRegisteredModules())
            {
                std::cout << m << "\n";
            }
            return 0;
        }

        // Run DPC checks
        if (("enterprise-profile" == mode)
            || ("protocol" == mode))
        {
            if (!args->Present("test"))
            {
                throw CommandException("devposture-proxy/enterprise-profile",
                                       "At least one --test must be provided");
            };

            // Build the DPC request object
            Json::Value request_content;
            request_content["ver"] = "1.0";
            request_content["correlation_id"] = generate_path_uuid("", '-');

            std::time_t t = std::time(nullptr);
            std::tm tm = *std::localtime(&t);
            std::ostringstream tstamp;
            tstamp << std::put_time(&tm, "%c %Z");
            request_content["timestamp"] = tstamp.str();

            // Add the checks requested
            for (uint32_t i = 0; i < args->GetValueLen("test"); i++)
            {
                request_content[args->GetValue("test", i)] = true;
            }

            // Finalize the request object
            Json::Value dpc_request;
            dpc_request["dpc_request"] = request_content;
            Json::StreamWriterBuilder builder;
            builder.settings_["indentation"] = "";
            std::string dpc_req_str = Json::writeString(builder, dpc_request);

            // Extract the DPC protocol to use
            std::string protocol;
            if ("enterprise-profile" == mode)
            {
                // In this mode, lookup the protocol for this enterprise profile

                std::string profile = args->GetValue("enterprise-profile", 0);
                protocol = proxy->ProtocolLookup(profile);
                std::cout << "*** ProtocolLookup(\"" + profile + "\"): "
                          << protocol << std::endl;
                auto alias_split = protocol.find(":");
                if (alias_split != std::string::npos)
                {
                    // If the lookup includes aliases, use the first element
                    std::string p = protocol.substr(0, alias_split);
                    protocol = p;
                }
            }
            else
            {
                protocol = args->GetValue("protocol", 0);
            }

            // Run the DPC checks
            std::cout << "\n*** Using DPC protocol '" << protocol << "'" << std::endl;
            std::cout << "*** RunChecks(\"" << protocol << "\", \""
                      << dpc_req_str << "\"): " << std::endl
                      << proxy->RunChecks(protocol, dpc_req_str) << "\n";
            return 0;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception caught: " << e.what() << "\n";
        return 1;
    }
    return 0;
}


int main(int argc, char **argv)
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("devposture-proxy",
                                "Test program for Device Posture Checks",
                                devposture_proxy));
    cmd->AddOption("list-modules",
                   "List available device posture check modules");
    cmd->AddOption("enterprise-profile",
                   "PROFILE-NAME",
                   true,
                   "Run a device posture check locally based on an Enterprise Profile name");
    cmd->AddOption("protocol",
                   "PROTOCOL",
                   true,
                   "Run a device posture check locally using a specific protocol name");
    cmd->AddOption("test",
                   "TEST-NAME",
                   true,
                   "Which tests to run. Test names are defined in the protocol profile");
    try
    {
        return cmd->RunCommand("devposture-proxy", argc, argv);
    }
    catch (const CommandException &excp)
    {
        std::cout << simple_basename(argv[0])
                  << ": ** ERROR **  " << excp.what() << std::endl;
        return 2;
    }
}
