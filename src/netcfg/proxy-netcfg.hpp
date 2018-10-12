//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
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
    class Device;

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

        const std::string CreateVirtualInterface(const std::string& device_name);

        Device* getVirtualInterface(const std::string & path);
        std::vector<std::string> FetchInterfaceList();
        bool ProtectSocket(int socket, const std::string& remote, bool ipv6);
    };

    const std::string Manager::CreateVirtualInterface(const std::string& device_name)
    {
        Ping();
        try
        {
            GVariant *res = Call("CreateVirtualInterface",
                                 g_variant_new("(s)",
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
        catch (NetCfgProxyException&)
        {
            throw;
        }
        catch (std::exception& excp)
        {
            throw NetCfgProxyException("CreateVirtualInterface",
                                       excp.what());
        }
    }

    bool Manager::ProtectSocket(int socket, const std::string & remote, bool ipv6)
    {
        if (!CheckObjectExists())
        {
            throw NetCfgProxyException("ProtectSocket",
                                       "net.openvpn.v3.netcfg service unavailable");
        }

        bool ret;
        try {
            GVariant *res = CallSendFD("ProtectSocket",
                                       g_variant_new("(sb)",
                                                     remote.c_str(),
                                                     ipv6),
                                       socket);
            g_variant_get(res, "(b)", &ret);
            g_variant_unref(res);
        }
        catch (NetCfgProxyException&)
        {
            throw;
        }
        return ret;
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
        catch (NetCfgProxyException&)
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
     * Class representing a IPv4 or IPv6 network
     */
    class Network {
    public:
        Network(std::string networkAddress, unsigned int prefix,
                bool ipv6, bool exclude=false)
            : address(std::move(networkAddress)),
              prefix(prefix), ipv6(ipv6), exclude(exclude)
        {
        }

        std::string address;
        unsigned int prefix;
        bool ipv6;
        bool exclude;
    };

    /**
     *   Class replicating a specific D-Bus network device object
     */
    class Device : public DBusProxy,
                    public RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<Device> Ptr;

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
         * @param ip_address String representation of the IP Address
         * @param prefix Prefix length (CIDR)
         * @param gateway Gateway for this network
         * @param ipv6 Is this Address an IPv6 address
         */
        void AddIPAddress(const std::string& ip_address,
                            unsigned int prefix,
                            const std::string& gateway,
                            bool ipv6);


        /**
         *  Takes a vector containing route destinations which is
         *  to be routed via the VPN
         *
         * @param routes
         * @param gateway
         */
        void AddNetworks(const std::vector<Network> &networks);

        /**
         *  Takes a list of DNS server IP addresses to enlist as
         *  DNS resolvers on the system
         *
         * @param server_list
         */
        void AddDNS(const std::vector<std::string>& server_list);

        /**
         *  Takes a list of DNS server IP addresses to be removed from
         *  the DNS resolver list
         *
         * @param server_list
         */
        void RemoveDNS(const std::vector<std::string>& server_list);

        /**
         *  Takes a list of DNS search domains to be used on the system
         *
         * @param domains
         */
        void AddDNSSearch(const std::vector<std::string>& domains);

        /**
         *  Takes a list of DNS serach domains to be removed from the system
         *
         * @param domains
         */
        void RemoveDNSSearch(const std::vector<std::string>& domains);

        /**
         *  Creates and applies a configuration to this virtual interface.
         *
         *  If the interface has not been created yet, it will first create
         *  it before applying any Add/Remove settings on the device.  If
         *  the device was already activated, it only commits the last
         *  un-applied changes.
         *
         *  @return Tun file descript or -1 on error
         */
        int Establish();

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

        /**
         * Set the MTU for the device
         *
         * @param mtu  unsigned int containing the new MTU value
         */
        void SetMtu(unsigned int mtu);

        /**
         * Set The Layer of the device
         *
         * @param layer  unsigned int of the tunnel device layer type.
         *               Valid values are 2 (TAP) or 3 (TUN).
         */
        void SetLayer(unsigned int layer);


        /**
         * Set to have a default route installed and reroute the gw to avoid routing loops
         *
         * @param ipv6 if ipv4 or ipv6 should be redirected
         * @param value if it should be enabled for this protocol
         */
        void SetRerouteGw(bool ipv6, bool value);
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

        void SetRemoteAddress(const std::string& remote, bool ipv6);
    };


    Device* Manager::getVirtualInterface(const std::string & path)
    {
        return new Device(GetConnection(), path);
    }


    void Device::SetRemoteAddress(const std::string& remote, bool ipv6)
    {
        GVariant *r = Call("SetRemoteAddress",
                           g_variant_new("(sb)",
                                         remote.c_str(), ipv6));
        g_variant_unref(r);
    }


    void Device::AddIPAddress(const std::string& ip_address,
                              const unsigned int prefix,
                              const std::string& gateway,
                              bool ipv6)
    {
        GVariant *r = Call("AddIPAddress",
                           g_variant_new("(susb)",
                                         ip_address.c_str(), prefix,
                                         gateway.c_str(), ipv6));
        g_variant_unref(r);
    }


    void Device::AddNetworks(const std::vector<Network> &networks)
    {
        GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(subb)"));
        for (const auto& net : networks)
        {
            g_variant_builder_add(bld, "(subb)",
                                  net.address.c_str(), net.prefix,
                                  net.ipv6, net.exclude);
        }

        // DBus somehow wants this still wrapped being able to do this
        // with one builder would be to simple or not broken enough
        GVariant *res = Call("AddNetworks", GLibUtils::wrapInTuple(bld));

        g_variant_unref(res);
    }


    void Device::AddDNS(const std::vector<std::string>& server_list)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(server_list);
        GVariant *res = Call("AddDNS", list);
        g_variant_unref(res);
    }


    void Device::RemoveDNS(const std::vector<std::string>& server_list)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(server_list);
        GVariant *res = Call("RemoveDNS", list);
        g_variant_unref(res);
    }


    void Device::AddDNSSearch(const std::vector<std::string>& domains)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
        GVariant *res = Call("AddDNSSearch", list);
        g_variant_unref(res);
    }


    void Device::RemoveDNSSearch(const std::vector<std::string>& domains)
    {
        GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
        GVariant *res = Call("RemoveDNSSearch", list);
        g_variant_unref(res);
    }


    int Device::Establish()
    {
        gint fd = -1;
        GVariant *res = CallGetFD("Establish", fd);
        g_variant_unref(res);
        return fd;
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


    void Device::SetLayer(unsigned int layer)
    {
        SetProperty("layer", layer);
    }


    void Device::SetMtu(unsigned int mtu)
    {
        SetProperty("mtu", mtu);
    }


    uid_t Device::GetOwner()
    {
        return GetUIntProperty("owner");
    }

    void Device::SetRerouteGw(bool ipv6, bool value)
    {
        if (ipv6)
        {
            SetProperty("reroute_ipv6", value);
        }
        else
        {
            SetProperty("reroute_ipv4", value);
        }
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
        return (NetCfgDeviceType) GetUIntProperty("layer");
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
