//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2019         OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
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
 * @file   netcfg.cpp
 *
 * @brief  Management commands for the net.openvpn.v3.netcfg service
 */

#include "dbus/core.hpp"

#include "common/cmdargparser.hpp"
#include "common/configfileparser.hpp"
#include "common/lookup.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "netcfg/netcfg-configfile.hpp"
#include "netcfg/proxy-netcfg.hpp"



int cmd_netcfg_service(ParsedArgs::Ptr args)
{
    DBus dbuscon(G_BUS_TYPE_SYSTEM);
    dbuscon.Connect();
    NetCfgProxy::Manager::Ptr prx;

    try
    {
        prx = new NetCfgProxy::Manager(dbuscon.GetConnection());
    }
    catch(const DBusException&)
    {
        // If --config-file-override is present, we expect
        // the user to want to do configuration changes directly
        // and do not depend on a connection to the
        // netcfg D-Bus service
        if (!args->Present("config-file-override"))
        {
            std::cerr << "Could not connect to the OpenVPN 3 "
                      << "Network Configuration service"
                      << std::endl;
            return 3;
        }
    }
    catch(const NetCfgProxyException& excp)
    {
        std::cerr << "Could not reach OpenVPN 3 Network Configuration Service: "
                  << excp.GetError() << std::endl;
        return 3;
    }

    try
    {
        DBusConnectionCreds creds(dbuscon.GetConnection());
        args->CheckExclusiveOptions({{"unsubscribe", "list-subscribers",
                                      "config-show", "config-set",
                                      "config-unset"}});

        if (args->Present("unsubscribe"))
        {
            std::string sub = args->GetValue("unsubscribe", 0);
            prx->NotificationUnsubscribe(sub);
            std::cout << "Unsubscribed '" << sub << "'" << std::endl;

            return 0;
        }

        if (args->Present("list-subscribers"))
        {
            std::cout << "Current subsribers: " << std::endl;

            for (const auto& sub : prx->NotificationSubscriberList())
            {
                std::cout << "- " << sub.first
                          << " (PID "
                          << std::to_string(creds.GetPID(sub.first)) << ")"
                          << std::endl;

                for (const auto& e : NetCfgChangeEvent::FilterMaskList(sub.second))
                {
                    std::cout << "        " << e << std::endl;
                }
                std::cout << std::endl;
            }
            return 0;
        }

        //
        // Options for managing the netcfg configuration file
        //
        try
        {
            std::vector<std::string> cfgopts = {"config-show",
                                                "config-set", "config-unset"};
            std::string cfgmode = args->Present(cfgopts);
            std::string config_file{};
            NetCfgConfigFile config;

            try
            {
                if (!args->Present("config-file-override"))
                {
                    config_file = prx->GetConfigFile();
                }
                else
                {
                    config_file = args->GetLastValue("config-file-override");
                }
                std::cout << "Loading configuration file: " << config_file
                          << std::endl;
                config.Load(config_file);
            }
            catch (const ConfigFileException& excp)
            {
                if ("config-show" == cfgmode)
                {
                    std::cout << excp.what()
                              << std::endl;
                    return 2;
                }
            }

            // Report any issues with the configuration file, regardless
            // of operation
            try
            {
                config.CheckExclusiveOptions();
            }
            catch (const ExclusiveOptionError& err)
            {
                std::cerr << std::endl
                          << "==================================="
                          << "==================================="
                          << std::endl;
                std::cerr << "An error was found in the configuration file:"
                          << std::endl;
                std::cerr << " *** " << err.what() << std::endl;
                std::cerr << "==================================="
                          << "==================================="
                          << std::endl << std::endl;
            }

            if ("config-show" == cfgmode)
            {
                if (!config.empty())
                {
                    std::cout << config;
                }
                else
                {
                    std::cout << "Configuration file is empty" << std::endl;
                }
            }
            else if ("config-set" == cfgmode
                     || "config-unset" == cfgmode)
            {
                std::string optname = args->GetLastValue(cfgmode);
                std::vector<std::string> values = args->GetAllExtraArgs();
                std::string new_value{""};

                // Simple sanity to retrieve the new config value
                if ("config-set" == cfgmode && values.size() == 0)
                {
                    throw CommandException("netcfg-service",
                                           "A value must be given to --"
                                           + cfgmode + " " + optname);
                }
                else if ("config-unset" == cfgmode && values.size() > 0)
                {
                    throw CommandException("netcfg-service",
                                           "No value can be given to --"
                                           + cfgmode + " " + optname);
                }
                else if (values.size() > 1)
                {
                    throw CommandException("netcfg-service",
                                           "Only a single value can be given to --"
                                           + cfgmode + " " + optname);
                }
                else if (values.size() == 1)
                {
                    new_value = values.at(0);
                }

                // Update the configuration and save the file.
                if ("config-unset" == cfgmode)
                {
                    config.UnsetOption(optname);
                }
                else
                {
                    config.SetValue(optname, new_value);
                }
                try
                {
                    config.CheckExclusiveOptions();
                    config.Save(config_file);
                    std::cout << "Configuration file updated.  "
                              << "Changes will be activated next time "
                              << "openvpn3-service-netcfg restarts"
                              << std::endl;
                }
                catch (const ExclusiveOptionError& err)
                {
                    std::cerr << "Configuration NOT changed due to the "
                              << "following error:" << std::endl << std::endl;
                    std::cerr << " *** " << err.what() << std::endl
                              << std::endl;
                }
            }
        }
        catch (const OptionNotFound&)
        {
            // Nothing to do; options not present
        }
        return 0;
    }
    catch (const ExclusiveOptionError& excp)
    {
        throw CommandException("netcfg-service", excp.what());
    }
    catch (const DBusException& excp)
    {
        throw CommandException("netcfg-service", excp.what());
    }
    catch (const DBusProxyAccessDeniedException& excp)
    {
        throw CommandException("netcfg-service", excp.what());
    }
}



std::string arghelper_netcfg_subscribers()
{
    DBusProxy prx(G_BUS_TYPE_SYSTEM,
                  OpenVPN3DBus_name_netcfg,
                  OpenVPN3DBus_interf_netcfg,
                  OpenVPN3DBus_rootp_netcfg);

    GVariant *l = prx.Call("NotificationSubscriberList");
    GVariantIter *list = nullptr;
    g_variant_get(l, "(a(su))", &list);

    GVariant *sub = nullptr;
    std::string res;
    while ((sub = g_variant_iter_next_value(list)))
    {
        gchar *subscriber = nullptr;
        unsigned int mask;
        g_variant_get(sub, "(su)", &subscriber, &mask);

        res += std::string(subscriber) + " ";
        g_free(subscriber);
        g_variant_unref(sub);
    }
    g_variant_iter_free(list);
    g_variant_unref(l);

    return res;
}


std::string arghelper_netcfg_config_keys()
{
    NetCfgConfigFile cfg;
    std::stringstream r;
    for (const auto& o : cfg.GetOptions(true))
    {
        r << o << " ";
    }
    return r.str();
}


SingleCommand::Ptr prepare_command_netcfg_service()
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("netcfg-service",
                                "Management of net.openvpn.v3.netcfg "
                                "(requires root)",
                                cmd_netcfg_service));
    cmd->AddOption("config-show",
                   "Show the current configuration file used by netcfg-service");
    cmd->AddOption("config-set", 0, "CONFIG-KEY", true,
                   "Sets a configuration option and saves it in the config file",
                   arghelper_netcfg_config_keys);
    cmd->AddOption("config-unset", 0, "CONFIG-KEY", true,
                   "Removes a configuration option and updates the config file",
                   arghelper_netcfg_config_keys);
    cmd->AddOption("config-file-override", 0, "CONFIG-FILE", true,
                   "Overrides the default configuration file (default file "
                   "provivded by openvpn3-service-netcfg)");
    cmd->AddOption("list-subscribers",
                   "List all D-Bus services subscribed to "
                   "NetworkChange signals");
    cmd->AddOption("unsubscribe", 0, "DBUS-UNIQUE-NAME", true,
                   "Unsubscribe a specific subscriber",
                   arghelper_netcfg_subscribers);

    return cmd;
}


