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
 * @file   proxy-netcfg.hpp
 *
 * @brief  D-Bus proxy for the net.openvpn.v3.netcfg service
 */

#pragma once

#include <string>
#include <vector>

#include <openvpn/common/rc.hpp>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "dbus/glibutils.hpp"
#include "netcfg-device.hpp"
#include "netcfg-exception.hpp"

using namespace openvpn;

namespace NetCfgProxy
{
    class Manager : public DBusProxy,
                    public RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<Manager> Ptr;

        /**
         *  Initialize the Network Configuration proxy for the
         *  main management interface
         *
         * @param dbuscon  D-Bus connection to use for D-Bus calls
         */
        Manager(GDBusConnection *dbuscon)
        : DBusProxy(dbuscon,
                        OpenVPN3DBus_name_netcfg,
                        OpenVPN3DBus_interf_netcfg,
                        OpenVPN3DBus_rootp_netcfg)
        {
        }

        const std::string CreateVirtualInterface(const NetCfgDeviceType& type,
                                                 const std::string& device_name);
        std::vector<std::string> FetchInterfaceList();
    };


    const std::string Manager::CreateVirtualInterface(const NetCfgDeviceType& type,
                                                      const std::string& device_name)
    {
        Ping();
        try
        {
            GVariant *res = Call("CreateVirtualInterface",
                                 g_variant_new("(us)",
                                               (guint) type,
                                               device_name.c_str()
                                               ));
            if (!res)
            {
                throw NetCfgProxyException("CreateVirtualInterface",
                                           "No results returned");
            }

            gchar *path = nullptr;
            g_variant_get(res, "(o)", &path);
            const std::string devpath(path);
            g_free(path);
            g_variant_unref(res);
            return devpath;
        }
        catch (NetCfgProxyException)
        {
            throw;
        }
        catch (std::exception& excp)
        {
            throw NetCfgProxyException("CreateVirtualInterface",
                                       excp.what());
        }
    }

    std::vector<std::string> Manager::FetchInterfaceList()
    {
        Ping();
        try
        {
            GVariant *res = Call("FetchInterfaceList");
            if (!res)
            {
                throw NetCfgProxyException("FetchInterfaceList",
                                           "No results returned");
            }

            GVariantIter *pathlist = nullptr;
            g_variant_get(res, "(ao)", &pathlist);

            GVariant *path = nullptr;
            std::vector<std::string> device_paths;
            while ((path = g_variant_iter_next_value(pathlist)))
            {
                gsize len;
                device_paths.push_back(std::string(g_variant_get_string(path, &len)));
                g_variant_unref(path);
            }
            g_variant_iter_free(pathlist);
            g_variant_unref(res);
            return device_paths;
        }
        catch (NetCfgProxyException)
        {
            throw;
        }
        catch (std::exception& excp)
        {
            throw NetCfgProxyException("CreateVirtualInterface",
                                       excp.what());
        }
    }


    /**
     *   Class replicating a specific D-Bus network device object
     */
    class Device : public DBusProxy,
                    public RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<Manager> Ptr;

        /**
         *  Initialize the Network Configuration proxy for
         *  accessing a specific network device
         *
         * @param dbuscon  D-Bus connection to use for D-Bus calls
         * @param devpath  D-Bus object path to the device to handle
         *
         */
        Device(GDBusConnection *dbuscon, const std::string& devpath)
        : DBusProxy(dbuscon,
                        OpenVPN3DBus_name_netcfg,
                        OpenVPN3DBus_interf_netcfg,
                        devpath)
        {
        }

        /**
         *  Adds an IPv4 address to this network device
         *
         * @param ip_address
         * @param prefix
         * @param broadcast
         */
        void AddIPv4Address(const std::string& ip_address,
                            const unsigned int prefix,
                            const std::string& broadcast);

        /**
         *  Removes an IPv4 address from this network device
         *
         * @param ip_address
         * @param prefix
         */
        void RemoveIPv4Address(const std::string& ip_address,
                               const unsigned int prefix);

        /**
         *  Adds an IPv6 address to this network device
         *
         * @param ip_address
         * @param prefix
         */
        void AddIPv6Address(const std::string& ip_address,
                            const unsigned int prefix);

        /**
         *  Removes an IPv6 address from this network device
         *
         * @param ip_address
         * @param prefix
         */
        void RemoveIPv6Address(const std::string& ip_address,
                               const unsigned int prefix);

        /**
         *  Takes a vector containing route destinations which is
         *  to be routed via the given gateway address
         *
         * @param routes
         * @param gateway
         */
        void AddRoutes(std::vector<std::string>& routes,
                       std::string& gateway);

        /**
         *  Takes a vector containing route destinations which is to
         *  be removed from the routing table
         *
         * @param routes
         * @param gateway
         */
        void RemoveRoutes(std::vector<std::string>& routes,
                          std::string& gateway);

        /**
         *  Takes a list of DNS server IP addresses to enlist as
         *  DNS resolvers on the system
         *
         * @param server_list
         */
        void AddDNS(std::vector<std::string>& server_list);

        /**
         *  Takes a list of DNS server IP addresses to be removed from
         *  the DNS resolver list
         *
         * @param server_list
         */
        void RemoveDNS(std::vector<std::string>& server_list);

        /**
         *  Takes a list of DNS search domains to be used on the system
         *
         * @param domains
         */
        void AddDNSSearch(std::vector<std::string>& domains);

        /**
         *  Takes a list of DNS serach domains to be removed from the system
         *
         * @param domains
         */
        void RemoveDNSSearch(std::vector<std::string>& domains);

        /**
         *  Creates and applies a configuration to this virtual interface.
         *
         *  If the interface has not been created yet, it will first create
         *  it before applying any Add/Remove settings on the device.  If
         *  the device was already activated, it only commits the last
         *  un-applied changes.
         */
        void Activate();

        /**
         *  Disables a virtual device, while preserving the configuration.
         *
         *  This will remove the virtual interface from the system and
         *  reverse any routes or DNS settings.  These settings can be
         *  activated again by calling the @Activate() method again.
         */
        void Disable();

        /**
         *   Destroys and completely removes this virtual network interface.
         */
        void Destroy();


        /*
         *  Generic functions for processing various properties
         */
        unsigned int GetLogLevel();
        void SetLogLevel(unsigned int lvl);

        uid_t GetOwner();
        std::vector<uid_t> GetACL();

        NetCfgDeviceType GetDeviceType();
        std::string GetDeviceName();
        bool GetActive();

        std::vector<std::string> GetIPv4Addresses();
        std::vector<std::string> GetIPv4Routes();
        std::vector<std::string> GetIPv6Addresses();
        std::vector<std::string> GetIPv6Routes();

        std::vector<std::string> GetDNS();
        std::vector<std::string> GetDNSSearch();
    };


    void Device::AddIPv4Address(const std::string& ip_address,
                                const unsigned int prefix,
                                const std::string& broadcast = "")
    {
        GVariant *r = Call("AddIPv4Address",
                           g_variant_new("(sus)",
                                         ip_address.c_str(),
                                         prefix,
                                         broadcast.c_str()));
        g_variant_unref(r);
    }


    void Device::RemoveIPv4Address(const std::string& ip_address,
                                   const unsigned int prefix)
    {
        GVariant *r = Call("RemoveIPv4Address",
                           g_variant_new("(su)",
                                         ip_address.c_str(),
                                         prefix));
        g_variant_unref(r);
    }


    void Device::AddIPv6Address(const std::string& ip_address,
                                const unsigned int prefix)
    {
        GVariant *r = Call("AddIPv6Address",
                           g_variant_new("(su)",
                                         ip_address.c_str(),
                                         prefix));
        g_variant_unref(r);
    }


    void Device::RemoveIPv6Address(const std::string& ip_address,
                                   const unsigned int prefix)
    {
        GVariant *r = Call("RemoveIPv6Address",
                           g_variant_new("(su)",
                                         ip_address.c_str(),
                                         prefix));
        g_variant_unref(r);
    }


    void Device::AddRoutes(std::vector<std::string>& routes,
                           std::string& gateway)
    {
        GVariant *rts = GLibUtils::GVariantFromVector(routes);
        GVariant *res = Call("AddRoutes",
                             g_variant_new("(ass)",
                                           rts, gateway.c_str()));
        g_variant_unref(res);
    }


    void Device::RemoveRoutes(std::vector<std::string>& routes,
                              std::string& gateway)
    {
        GVariant *rts = GLibUtils::GVariantFromVector(routes);
        GVariant *res = Call("RemoveRoutes",
                             g_variant_new("(ass)",
                                           rts, gateway.c_str()));
        g_variant_unref(res);
    }


    void Device::AddDNS(std::vector<std::string>& server_list)
    {
        GVariant *list = GLibUtils::GVariantFromVector(server_list);
        GVariant *res = Call("AddDNS", g_variant_new("(as)", list));
        g_variant_unref(res);
    }


    void Device::RemoveDNS(std::vector<std::string>& server_list)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(server_list);
        GVariant *res = Call("RemoveDNS", list);
        g_variant_unref(res);
    }


    void Device::AddDNSSearch(std::vector<std::string>& domains)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
        GVariant *res = Call("AddDNSSearch", list);
        g_variant_unref(res);
    }


    void Device::RemoveDNSSearch(std::vector<std::string>& domains)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
        GVariant *res = Call("RemoveDNSSearch", list);
        g_variant_unref(res);
    }


    void Device::Activate()
    {
        GVariant *res = Call("Activate");
        g_variant_unref(res);
    }


    void Device::Disable()
    {
        GVariant *res = Call("Disable");
        g_variant_unref(res);

    }

    void Device::Destroy()
    {
        GVariant *res = Call("Destroy");
        g_variant_unref(res);
    }


    unsigned int Device::GetLogLevel()
    {
        return GetUIntProperty("log_level");
    }


    void Device::SetLogLevel(unsigned int lvl)
    {
        SetProperty("log_level", lvl);
    }


    uid_t Device::GetOwner()
    {
        return GetUIntProperty("owner");
    }


    std::vector<uid_t> Device::GetACL()
    {
        GVariant *res = GetProperty("acl");
        if (NULL == res)
        {
            THROW_DBUSEXCEPTION("NetCfgProxy::Device",
                                "GetACL() call failed");
        }
        GVariantIter *acl = NULL;
        g_variant_get(res, "au", &acl);

        GVariant *uid = NULL;
        std::vector<uid_t> ret;
        while ((uid = g_variant_iter_next_value(acl)))
        {
            ret.push_back(g_variant_get_uint32(uid));
            g_variant_unref(uid);
        }
        g_variant_unref(res);
        g_variant_iter_free(acl);
        return ret;
    }


    NetCfgDeviceType Device::GetDeviceType()
    {
        return (NetCfgDeviceType) GetUIntProperty("device_type");
    }


    std::string Device::GetDeviceName()
    {
        return GetStringProperty("device_name");

    }


    bool Device::GetActive()
    {
        return GetBoolProperty("active");
    }


    std::vector<std::string> Device::GetIPv4Addresses()
    {
        std::vector<std::string> ret;
        return ret;
    }


    std::vector<std::string> Device::GetIPv4Routes()
    {
        std::vector<std::string> ret;
        return ret;
    }


    std::vector<std::string> Device::GetIPv6Addresses()
    {
        std::vector<std::string> ret;
        return ret;
    }


    std::vector<std::string> Device::GetIPv6Routes()
    {
        std::vector<std::string> ret;
        return ret;
    }


    std::vector<std::string> Device::GetDNS()
    {
        std::vector<std::string> ret;
        return ret;
    }


    std::vector<std::string> Device::GetDNSSearch()
    {
        std::vector<std::string> ret;
        return ret;
    }

} // namespace NetCfgProxy
