//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Arne Schwabe <arne@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  Antonio Quartulli <antonio@openvpn.net>
//

/**
 * @file   proxy-netcfg-device.hpp
 *
 * @brief  Declaration of the D-Bus proxy for the device objects
 *         of the net.openvpn.v3.netcfg service
 */

#pragma once
#define OPENVPN3_NETCFGPRX_DEVICE

#include "build-config.h"

#include <string>
#include <vector>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>

#ifdef ENABLE_OVPNDCO
#include <sys/types.h>
#include <sys/socket.h>

#include <openvpn/io/io.hpp>
#include <openvpn/addr/ip.hpp>
#include <openvpn/dco/key.hpp>
#endif


namespace NetCfgProxy {

#ifdef ENABLE_OVPNDCO
class DCO
{
  public:
    using Ptr = std::shared_ptr<DCO>;

    DCO(DBus::Proxy::Client::Ptr proxy_, const DBus::Object::Path &dcopath);

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
    void NewPeer(unsigned int peer_id,
                 int transport_fd,
                 const struct sockaddr *sa,
                 unsigned int salen,
                 const openvpn::IPv4::Addr &vpn4,
                 const openvpn::IPv6::Addr &vpn6) const;


    /**
     * Pass crypto configuration into kernel module.
     *
     * @param key_slot  key slot (OVPN_KEY_SLOT_PRIMARY or
     *                  OVPN_KEY_SLOT_SECONDARY)
     * @param kc        KeyConfig struct, which contains enc/dec keys,
     *                  cipher algorithm, cipher key size, nonces (for gcm),
     *                  hmac algorithm, hmacs and hmac key size (for cbc)
     */
    void NewKey(unsigned int key_slot,
                const openvpn::KoRekey::KeyConfig *kc) const;


    /**
     * Swaps primary key with secondary key
     *
     * @param peer_id ID of the peer to swap keys for
     */
    void SwapKeys(unsigned int peer_id) const;


    /**
     * @brief Sets properties of peer
     *
     * @param peer_id            ID of the peer to modify
     * @param keepalive_interval keepalive interval
     * @param keepalive_timeout  keepalive timeout
     */
    void SetPeer(unsigned int peer_id,
                 int keepalive_interval,
                 int keepalive_timeout) const;

  private:
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr dcotgt = nullptr;
};
#endif // ENABLE_OVPNDCO


/**
 * Class representing a IPv4 or IPv6 network
 */
class Network
{
  public:
    Network(std::string networkAddress,
            unsigned int prefix,
            bool ipv6,
            bool exclude = false);

    std::string address;
    unsigned int prefix;
    bool ipv6;
    bool exclude;
};

// FIXME: Migration hack - netcfg-device.hpp need refactoring
using NetCfgDeviceType = unsigned int;


/**
 *   Class replicating a specific D-Bus network device object
 */
class Device
{
  public:
    using Ptr = std::shared_ptr<Device>;

    /**
     *  Initialize the Network Configuration proxy for
     *  accessing a specific network device
     *
     * @param dbuscon  D-Bus connection to use for D-Bus calls
     * @param devpath  D-Bus object path to the device to handle
     *
     */
    Device(DBus::Connection::Ptr dbuscon, const DBus::Object::Path &devpath);


    /**
     * Adds bypass route to remote
     *
     * @param addr String representation of the IP Address
     * @param ipv6 Is this Address an IPv6 address
     * @return true if success
     */
    bool AddBypassRoute(const std::string &addr, bool ipv6) const;


    /**
     *  Adds an IPv4 address to this network device
     *
     * @param ip_address String representation of the IP Address
     * @param prefix Prefix length (CIDR)
     * @param gateway Gateway for this network
     * @param ipv6 Is this Address an IPv6 address
     */
    void AddIPAddress(const std::string &ip_address,
                      unsigned int prefix,
                      const std::string &gateway,
                      bool ipv6) const;


    /**
     *  Takes a vector containing route destinations which is
     *  to be routed via the VPN
     *
     * @param routes
     * @param gateway
     */
    void AddNetworks(const std::vector<Network> &networks) const;


    /**
     *  Changes the DNS query scope for the virtual interface
     *
     *  Valid values are:
     *    - global  -  (default mode) will be used to query all domains
     *    - tunnel  -  (split-dns) will only be used for lookup ups on the
     *                 registered DNS search domains.
     *
     *  @param scope   std::string with the DNS resolver scope
     */
    void SetDNSscope(const std::string &scope) const;


    /**
     *  Takes a list of DNS server IP addresses to enlist as
     *  DNS resolvers on the system
     *
     * @param server_list
     */
    void AddDNS(const std::vector<std::string> &server_list) const;


    /**
     *  Takes a list of DNS server IP addresses to be removed from
     *  the DNS resolver list
     *
     * @param server_list
     */
    void RemoveDNS(const std::vector<std::string> &server_list) const;


    /**
     *  Takes a list of DNS search domains to be used on the system
     *
     * @param domains
     */
    void AddDNSSearch(const std::vector<std::string> &domains) const;


    /**
     *  Takes a list of DNS serach domains to be removed from the system
     *
     * @param domains
     */
    void RemoveDNSSearch(const std::vector<std::string> &domains) const;


#ifdef ENABLE_OVPNDCO
    /**
     * Enables DCO functionality. This requires ovpn-dco kernel module.
     *
     * @param dev_name name of net device to create
     * @return DCO* DCO proxy object
     */
    DCO *EnableDCO(const std::string &dev_name) const;


    /**
     * Applies configuration to DCO interface.
     *
     */
    void EstablishDCO() const;
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
    int Establish() const;


    /**
     *  Disables a virtual device, while preserving the configuration.
     *
     *  This will remove the virtual interface from the system and
     *  reverse any routes or DNS settings.  These settings can be
     *  activated again by calling the @Activate() method again.
     */
    void Disable() const;


    /**
     *   Destroys and completely removes this virtual network interface.
     */
    void Destroy() const;


    /**
     * Set the MTU for the device
     *
     * @param mtu  unsigned int containing the new MTU value
     */
    void SetMtu(const uint16_t mtu) const;


    /**
     * Set The Layer of the device
     *
     * @param layer  unsigned int of the tunnel device layer type.
     *               Valid values are 2 (TAP) or 3 (TUN).
     */
    void SetLayer(unsigned int layer) const;


    /**
     * Set to have a default route installed and reroute the gw to avoid routing loops
     *
     * @param ipv6 if ipv4 or ipv6 should be redirected
     * @param value if it should be enabled for this protocol
     */
    void SetRerouteGw(bool ipv6, bool value) const;


    /*
     *  Generic functions for processing various properties
     */
    unsigned int GetLogLevel() const;
    void SetLogLevel(unsigned int lvl) const;


    uid_t GetOwner() const;
    std::vector<uid_t> GetACL() const;


    NetCfgDeviceType GetDeviceType() const;
    const std::string GetDeviceName() const;
    const DBus::Object::Path GetDevicePath() const;
    bool GetActive() const;

    std::vector<std::string> GetIPv4Addresses() const;
    std::vector<std::string> GetIPv4Routes() const;
    std::vector<std::string> GetIPv6Addresses() const;
    std::vector<std::string> GetIPv6Routes() const;

    std::vector<std::string> GetDNS() const;
    std::vector<std::string> GetDNSSearch() const;

    void SetRemoteAddress(const std::string &remote, bool ipv6) const;


  private:
    DBus::Connection::Ptr dbuscon = nullptr;
    DBus::Proxy::Client::Ptr proxy = nullptr;
    DBus::Proxy::TargetPreset::Ptr prxtgt = nullptr;
};

} // namespace NetCfgProxy
