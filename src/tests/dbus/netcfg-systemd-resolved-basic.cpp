//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2020         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg-systemd-resolved-basic.cpp
 *
 * @brief  Basic test programs for the NetCfg:DNS::resolver
 *         proxy implementation
 */


#include <iostream>
#include <sys/socket.h>

#include "common/cmdargparser.hpp"
#include "dbus/core.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"

using namespace NetCfg::DNS;

void print_details(resolved::Link::Ptr link)
{
    std::string curr_dns_srv{""};
    std::string default_route{""};
    std::vector<std::string> dns_srvs;
    resolved::SearchDomain::List srch;

    try
    {
        curr_dns_srv = link->GetCurrentDNSServer();
    }
    catch (const DBusException& excp)
    {
        std::cout << "** ERROR **  " << excp.what() << std::endl;
    }

    try
    {
        default_route = (link->GetDefaultRoute() ? "true" : " false");
    }
    catch (const std::exception& excp)
    {
        std::cout << "** ERROR **  " << excp.what() << std::endl;
        default_route = "(unknown)";
    }

    try
    {
        dns_srvs = link->GetDNSServers();
    }
    catch (const DBusException& excp)
    {
        std::cout << "** ERROR **  " << excp.what() << std::endl;
    }

    try
    {
        srch = link->GetDomains();
    }
    catch (const DBusException& excp)
    {
        std::cout << "** ERROR **  " << excp.what() << std::endl;
    }

    std::cout << "Current DNS server: " << curr_dns_srv
              << std::endl;

    std::cout << "Default route: " << default_route
              << std::endl;

    for (const auto& srv : dns_srvs)
    {
        std::cout << "DNS server: " << srv << std::endl;
    }

    for (const auto& dom : srch)
    {
        std::cout << "Domain: " << dom.search
                  << " routing: " << (dom.routing ? "true" : "false")
                  << std::endl;
    }

}


int program(ParsedArgs::Ptr args)
{
    std::vector<std::string> exargs = args->GetAllExtraArgs();
    if (exargs.size() != 1)
    {
        throw CommandException("", "Only one device name can be used");
    }

    DBus conn(G_BUS_TYPE_SYSTEM);
    conn.Connect();

    resolved::Manager srmgr(conn.GetConnection());
    resolved::Link::Ptr link = srmgr.RetrieveLink(exargs[0]);


    std::cout << "systemd-resolved path: " << link->GetPath() << std::endl;
    bool mods = args->Present("add-resolver") || args->Present("reset-resolver")
              || args->Present("add-search") || args->Present("reset-search")
              || args->Present("set-default-route") || args->Present("revert");
    if (mods)
    {
        std::cout << "Before changes: " << std::endl;
    }

    print_details(link);

    if (args->Present("add-resolver") || args->Present("reset-resolver"))
    {
        // Set a new DNS resolver server
        resolved::ResolverRecord::List rslv;

        if (args->Present("add-resolver"))
        {
            for (const auto& ip : args->GetAllValues("add-resolver"))
            {
                rslv.push_back(resolved::ResolverRecord(AF_INET, ip));
            }
        }
        link->SetDNSServers(rslv);
    }

    if (args->Present("add-search") || args->Present("reset-search"))
    {
        // Set DNS search domains
        resolved::SearchDomain::List srchs;

        if (args->Present("add-search"))
        {
            bool routing = args->Present("search-routing");

            for (const auto& s : args->GetAllValues("add-search"))
            {
                srchs.push_back(resolved::SearchDomain(s, routing));
            }
        }
        link->SetDomains(srchs);
    }

    if (args->Present("set-default-route"))
    {
        link->SetDefaultRoute(args->GetBoolValue("set-default-route", 0));
    }

    if (args->Present("revert"))
    {
        link->Revert();
    }

    if (mods)
    {
        std::cout << std::endl << "After changes: " << std::endl;
        print_details(link);
    }

    return 0;
}

int main(int argc, char **argv)
{
    SingleCommand cmd("netcfg-systemd-resolved",
                      "Test program for the systemd-resolved D-Bus API",
                      program);
    cmd.AddOption("add-resolver", "IP-ADDRESS", true,
                  "Add a DNS resolver (can be used multiple times)");
    cmd.AddOption("reset-resolver",
                  "Remove all DNS resolvers for this device");
    cmd.AddOption("add-search", "SEARCH-DOMAIN", true,
                  "Add a DNS search domain (can be used multiple times)");
    cmd.AddOption("reset-search",
                  "Remove all DNS search domains for this device");
    cmd.AddOption("search-routing", 0,
                  "Sets the routing flag for the SEARCH-DOMAIN being added");
    cmd.AddOption("set-default-route", "BOOL", true,
                  "Changes the DefaultRoute flag for the interface");
    cmd.AddOption("revert", 0,
                  "Revert all DNS settings on the interface to systemd-resovled defaults");

    try
    {
        return cmd.RunCommand(simple_basename(argv[0]), argc, argv);
    }
    catch (const CommandException& excp)
    {
        std::cout << excp.what() << std::endl;
        return 2;
    }
}
