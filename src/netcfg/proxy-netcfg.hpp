//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2019  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2019  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018         Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019         Lev Stipakov <lev@openvpn.net>
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
 * @brief  Declaration of the D-Bus proxy for the
 *         net.openvpn.v3.netcfg service
 */

#pragma once

#include <string>
#include <vector>

#include <openvpn/common/rc.hpp>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "netcfg-changeevent.hpp"
#include "netcfg-device.hpp"

using namespace openvpn;

namespace NetCfgProxy
{
    class Device;

#ifdef ENABLE_OVPNDCO
    class DCO;
#endif

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
        Manager(GDBusConnection *dbuscon);

        const std::string GetConfigFile();

        const std::string CreateVirtualInterface(const std::string& device_name);

        Device* getVirtualInterface(const std::string & path);
        std::vector<std::string> FetchInterfaceList();
        bool ProtectSocket(int socket, const std::string& remote, bool ipv6, const std::string& devpath);
        bool DcoAvailable();
        void Cleanup();

        void NotificationSubscribe(NetCfgChangeType filter_flags);
        void NotificationUnsubscribe(const std::string& subscriber);
        void NotificationUnsubscribe();
        NetCfgSubscriptions::NetCfgNotifSubscriptions NotificationSubscriberList();
    };



    /**
     * Class representing a IPv4 or IPv6 network
     */
    class Network {
    public:
        Network(std::string networkAddress, unsigned int prefix,
                bool ipv6, bool exclude=false);

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
        Device(GDBusConnection *dbuscon, const std::string& devpath);

        /**
         * Adds bypass route to remote
         *
         * @param addr String representation of the IP Address
         * @param ipv6 Is this Address an IPv6 address
         * @return true if success
         */
        bool AddBypassRoute(const std::string& addr, bool ipv6);

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

#ifdef ENABLE_OVPNDCO
        /**
         * Enables DCO functionality. This requires ovpn-dco kernel module.
         *
         * @param dev_name name of net device to create
         * @return DCO* DCO proxy object
         */
        DCO* EnableDCO(const std::string& dev_name);

        /**
         * Applies configuration to DCO interface.
         *
         */
        void EstablishDCO();
#endif

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

#ifdef ENABLE_OVPNDCO
    class DCO : public DBusProxy, public RC<thread_unsafe_refcount>
    {
    public:
        typedef RCPtr<DCO> Ptr;

        DCO(GDBusConnection *dbuscon, const std::string& dcopath);

        /**
         * Returns file descriptor used by unprivileged process to
         * communicate to kernel module.
         *
         * @return int file descriptor, -1 for failure
         */
        int GetPipeFD();

        /**
         * @brief Creates a new peer in ovpn-dco kernel module.
         *
         * @param peer_id      ID of the peer to create
         * @param transport_fd fd of transport socket, provided by client
         * @param sa           sockaddr object indentifying the remote endpoint
         * @param salen        the length of the 'sa' object
         * @param vpn4         IPv4 of this peer in the tunnel
         * @param vpn6         IPV6 of this peer in the tunnel
         */
        void NewPeer(unsigned int peer_id, int transport_fd,
                     const sockaddr *sa, unsigned int salen,
                     const IPv4::Addr& vpn4, const IPv6::Addr& vpn6);


        /**
         * Pass crypto configuration into kernel module.
         *
         * @param key_slot  key slot (OVPN_KEY_SLOT_PRIMARY or
         *                  OVPN_KEY_SLOT_SECONDARY)
         * @param kc        KeyConfig struct, which contains enc/dec keys,
         *                  cipher algorithm, cipher key size, nonces (for gcm),
         *                  hmac algorithm, hmacs and hmac key size (for cbc)
         */
        void NewKey(unsigned int key_slot, const KoRekey::KeyConfig* kc);

        /**
         * Swaps primary key with secondary key
         *
         * @param peer_id ID of the peer to swap keys for
         */
        void SwapKeys(unsigned int peer_id);

        /**
         * @brief Sets properties of peer
         *
         * @param peer_id            ID of the peer to modify
         * @param keepalive_interval keepalive interval
         * @param keepalive_timeout  keepalive timeout
         */
        void SetPeer(unsigned int peer_id, int keepalive_interval,
                     int keepalive_timeout);
    };
#endif  // ENABLE_OVPNDCO
} // namespace NetCfgProxy
