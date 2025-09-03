//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   version-service.cpp
 *
 * @brief  Retrieves the version strings of each of the backend D-Bus services
 */

#include <iostream>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/credentials/query.hpp>

#include "common/cmdargparser.hpp"
#include "common/utils.hpp"
#include "dbus/constants.hpp"
#include "configmgr/proxy-configmgr.hpp"
#include "log/proxy-log.hpp"
#include "netcfg/proxy-netcfg-mgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"


class ServiceProxy
{
  public:
    ServiceProxy(DBus::Connection::Ptr dbusconn,
                 const std::string &busname,
                 const DBus::Object::Path &srvpath,
                 const std::string &srvinterf);

    std::string GetVersion();
    std::string GetBinaryName() const;
    bool Failed() const;

    friend std::ostream &operator<<(std::ostream &os, ServiceProxy &sprx)
    {
        std::string binary = sprx.GetBinaryName();
        std::string version = sprx.GetVersion();
        return os << binary << ":"
                  << std::setw(40 - binary.length())
                  << std::setfill(' ') << " " << version;
    }


  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr service_tgt = nullptr;
    DBus::Proxy::Utils::Query::Ptr prxqry = nullptr;
    DBus::Credentials::Query::Ptr credsqry = nullptr;
    bool failed = false;
};

ServiceProxy::ServiceProxy(DBus::Connection::Ptr dbusconn,
                           const std::string &srvbusname,
                           const DBus::Object::Path &srvpath,
                           const std::string &srvinterface)
{
    try
    {
        proxy = DBus::Proxy::Client::Create(dbusconn, srvbusname);
        service_tgt = DBus::Proxy::TargetPreset::Create(srvpath, srvinterface);
        prxqry = DBus::Proxy::Utils::Query::Create(proxy);
        credsqry = DBus::Credentials::Query::Create(dbusconn);
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        failed = true;
    }
}


std::string ServiceProxy::GetVersion()
{
    if (!proxy)
    {
        return "(inaccessible)";
    }
    try
    {
        return prxqry->ServiceVersion(service_tgt->object_path,
                                      service_tgt->interface);
    }
    catch (const DBus::Proxy::Exception &excp)
    {
        failed = true;
        return "(unavailable)";
    }
}


std::string ServiceProxy::GetBinaryName() const
{
    if (!proxy)
    {
        return "(unknown binary)";
    }

    //  Extract the executable binary filename for this service by
    //  identifying the PID the process of the service is known by.
    //
    //  With the PID value, the /proc/$PID/cmdline file can be read
    //  to retrieve the information about the executable.
    //
    pid_t srv_pid = credsqry->GetPID(proxy->GetDestination());
    std::string cmdfile = "/proc/" + std::to_string(srv_pid) + "/cmdline";

    // The 'cmdline' file is separated by NULL terminators, so read until
    // the first NULL terminator to get the executable name.  This
    // will include the full path to the executable - the simple_basename()
    // strips away the path itself.
    std::ifstream procfile(cmdfile, std::ios::binary);
    char cmd[513] = {};
    procfile.get(cmd, 512, '\0');
    return simple_basename(std::string(cmd));
}


bool ServiceProxy::Failed() const
{
    return failed;
}


/**
 *  Dump various version information, both about the openvpn3 command
 *  but also about the OpenVPN 3 Core library.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 *
 * @return Returns the exit code which will be returned to the calling shell
 */
int cmd_version_services(ParsedArgs::Ptr args)
{
    if (args->Present("services"))
    {

        std::cout << "OpenVPN 3 D-Bus services:" << std::endl
                  << std::endl;

        auto dbusconn = DBus::Connection::Create(DBus::BusType::SYSTEM);

        ServiceProxy backendstart(dbusconn,
                                  Constants::GenServiceName("backends"),
                                  Constants::GenPath("backends"),
                                  Constants::GenInterface("backends"));
        std::cout << "  - Client backend starter service" << std::endl
                  << "    " << backendstart << std::endl
                  << std::endl;

        ServiceProxy cfgmgr(dbusconn,
                            Constants::GenServiceName("configuration"),
                            Constants::GenPath("configuration"),
                            Constants::GenInterface("configuration"));
        std::cout << "  - Configuration Service" << std::endl
                  << "    " << cfgmgr << std::endl
                  << std::endl;

        ServiceProxy logsrv(dbusconn,
                            Constants::GenServiceName("log"),
                            Constants::GenPath("log"),
                            Constants::GenInterface("log"));
        std::cout << "  - Log Service" << std::endl
                  << "    " << logsrv << std::endl
                  << std::endl;

        ServiceProxy netcfg(dbusconn,
                            Constants::GenServiceName("netcfg"),
                            Constants::GenPath("netcfg"),
                            Constants::GenInterface("netcfg"));
        std::cout << "  - Network Configuration Service" << std::endl
                  << "    " << netcfg << std::endl
                  << std::endl;

        ServiceProxy sessmgr(dbusconn,
                             Constants::GenServiceName("sessions"),
                             Constants::GenPath("sessions"),
                             Constants::GenInterface("sessions"));
        std::cout << "  - Session Manager Service" << std::endl
                  << "    " << sessmgr << std::endl
                  << std::endl;

        if (backendstart.Failed()
            || cfgmgr.Failed()
            || logsrv.Failed()
            || netcfg.Failed()
            || sessmgr.Failed())
        {
            std::cout << "** Some errors occurred retrieving version information."
                      << std::endl
                      << "** Ensure you run this command as the root or "
                      << OPENVPN_USERNAME << " user."
                      << std::endl
                      << std::endl;
            return 2;
        }

        return 0;
    }
    std::cout << get_version("/" + args->GetArgv0()) << std::endl;
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'version' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_version_services()
{
    SingleCommand::Ptr version;
    version.reset(new SingleCommand("version",
                                    "Show program version information",
                                    cmd_version_services));
    version->AddOption("services",
                       "List all OpenVPN 3 D-Bus services and their version");

    return version;
}
