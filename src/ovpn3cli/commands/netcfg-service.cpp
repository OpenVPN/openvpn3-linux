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
#include "common/lookup.hpp"
#include "netcfg/netcfg-changeevent.hpp"
#include "netcfg/proxy-netcfg.hpp"

int cmd_netcfg_service(ParsedArgs args)
{
    DBus dbuscon(G_BUS_TYPE_SYSTEM);
    dbuscon.Connect();
    NetCfgProxy::Manager prx(dbuscon.GetConnection());

    try
    {
        if (args.Present("unsubscribe"))
        {
            std::string sub = args.GetValue("unsubscribe", 0);
            prx.NotificationUnsubscribe(sub);
            std::cout << "Unsubscribed '" << sub << "'" << std::endl;
        }

        if (args.Present("list-subscribers"))
        {
            std::cout << "Current subsribers: " << std::endl;

            for (const auto& sub : prx.NotificationSubscriberList())
            {
                std::cout << "- " << sub.first << ": " << std::endl;
                for (const auto& e : NetCfgChangeEvent::FilterMaskList(sub.second))
                {
                    std::cout << "        " << e << std::endl;
                }
                std::cout << std::endl;
            }
        }
    }
    catch (const DBusException& excp)
    {
        throw CommandException("netcfg-service", excp.what());
    }
    catch (const DBusProxyAccessDeniedException& excp)
    {
        throw CommandException("netcfg-service", excp.what());
    }
    return 0;
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


SingleCommand::Ptr prepare_command_netcfg_service()
{
    SingleCommand::Ptr cmd;
    cmd.reset(new SingleCommand("netcfg-service",
                                "Management of net.openvpn.v3.netcfg "
                                "(requires root)",
                                cmd_netcfg_service));
    cmd->AddOption("list-subscribers",
                   "List all D-Bus services subscribed to "
                   "NetworkChange signals");
    cmd->AddOption("unsubscribe", 0, "DBUS-UNIQUE-NAME", true,
                   "Unsubscribe a specific subscriber",
                   arghelper_netcfg_subscribers);

    return cmd;
}


