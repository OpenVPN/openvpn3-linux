//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   version.cpp
 *
 * @brief  Simple generic command for displaying the program version
 */

#include <iostream>
#include "common/cmdargparser.hpp"

#ifdef OVPN3CLI_OPENVPN3ADMIN
#include "configmgr/proxy-configmgr.hpp"
#include "log/proxy-log.hpp"
#include "netcfg/proxy-netcfg-mgr.hpp"
#include "sessionmgr/proxy-sessionmgr.hpp"
#endif


#ifdef OVPN3CLI_OPENVPN3ADMIN
bool failed = false; //<< Is true if the RetrieveServiceVersion() call failed

/**
 *  Retrieve the service version information, if available.  It will not
 *  cause any instant failures if the service does not respond as expected.
 *
 * @param prx  DBusProxy object which provides the GetServiceVersion() method
 *
 * @return Returns a std::string containing the version reference or a
 *         generic explanation if information could not be determined.
 */
static std::string RetrieveServiceVersion(DBusProxy prx)
{
    try
    {
        return prx.GetServiceVersion();
    }
    catch (DBusProxyAccessDeniedException &excp)
    {
        failed = true;
        return "(unavailable - no access)";
    }
    catch (DBusException &excp)
    {
        failed = true;
        return "(unavailable)";
    }
}
#endif


/**
 *  Dump various version information, both about the openvpn3 command
 *  but also about the OpenVPN 3 Core library.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 *
 * @return Returns the exit code which will be returned to the calling shell
 */
int cmd_version(ParsedArgs::Ptr args)
{
#ifdef OVPN3CLI_OPENVPN3ADMIN
    if (args->Present("services"))
    {

        std::cout << "OpenVPN 3 D-Bus services:" << std::endl
                  << std::endl;

        DBus dbcon(G_BUS_TYPE_SYSTEM);
        dbcon.Connect();

        DBusProxy bckstprx(dbcon.GetConnection(),
                           OpenVPN3DBus_name_backends,
                           OpenVPN3DBus_interf_backends,
                           OpenVPN3DBus_rootp_backends);
        std::cout << "  - Client backend starter service" << std::endl
                  << "     openvpn3-service-backendstart: "
                  << RetrieveServiceVersion(bckstprx) << std::endl
                  << std::endl;

        OpenVPN3ConfigurationProxy cfgprx(dbcon.GetConnection(),
                                          OpenVPN3DBus_rootp_configuration);
        std::cout << "  - Configuration Service" << std::endl
                  << "     openvpn3-service-configmgr:    "
                  << RetrieveServiceVersion(cfgprx) << std::endl
                  << std::endl;

        LogServiceProxy logsrvprx(dbcon.GetConnection());
        std::cout << "  - Log Service" << std::endl
                  << "     openvpn3-service-logger:       "
                  << RetrieveServiceVersion(logsrvprx) << std::endl
                  << std::endl;

        NetCfgProxy::Manager netcfgprx(dbcon.GetConnection());
        std::cout << "  - Network Configuration Service" << std::endl
                  << "     openvpn3-service-netcfg:       "
                  << RetrieveServiceVersion(netcfgprx) << std::endl
                  << std::endl;

        OpenVPN3SessionProxy sessprx(dbcon.GetConnection(), OpenVPN3DBus_rootp_sessions);
        std::cout << "  - Session Manager Service" << std::endl
                  << "     openvpn3-service-sessionmgr:   "
                  << RetrieveServiceVersion(sessprx) << std::endl
                  << std::endl;

        if (failed)
        {
            std::cout << "** Some errors occured retrieving version information."
                      << std::endl
                      << "** Ensure you run this command as the root or "
                      << OPENVPN_USERNAME << " user."
                      << std::endl
                      << std::endl;
            return 2;
        }

        return 0;
    }
#endif
    std::cout << get_version("/" + args->GetArgv0()) << std::endl;
    return 0;
}


/**
 *  Creates the SingleCommand object for the 'version' command
 *
 * @return  Returns a SingleCommand::Ptr object declaring the command
 */
SingleCommand::Ptr prepare_command_version()
{
    SingleCommand::Ptr version;
    version.reset(new SingleCommand("version",
                                    "Show program version information",
                                    cmd_version));
#ifdef OVPN3CLI_OPENVPN3ADMIN
    version->AddOption("services",
                       "List all OpenVPN 3 D-Bus services and their version");
#endif

    return version;
}
