//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018 - 2022  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019 - 2022  Lev Stipakov <lev@openvpn.net>
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
 * @file   proxy-netcfg-device.cpp
 *
 * @brief  Implementation of D-Bus proxy for the device objects
 *         of the net.openvpn.v3.netcfg service
 */

#include <string>
#include <vector>

#define OPENVPN_EXTERN extern
#include <openvpn/common/rc.hpp>
#include <openvpn/common/base64.hpp>
#include <openvpn/log/logbase.hpp>

#include "dbus/core.hpp"
#include "dbus/proxy.hpp"
#include "dbus/glibutils.hpp"
#include "proxy-netcfg-device.hpp"
#include "netcfg-device.hpp"
#include "netcfg-exception.hpp"
#ifdef ENABLE_OVPNDCO
#include "dco-keyconfig.pb.h"
#endif

using namespace openvpn;


namespace NetCfgProxy {
//
//  class NetCfgProxy::Network
//

Network::Network(std::string networkAddress, unsigned int prefix, bool ipv6, bool exclude)
    : address(std::move(networkAddress)),
      prefix(prefix), ipv6(ipv6), exclude(exclude)
{
}



//
//  class NetCfgProxy::Device
//

Device::Device(GDBusConnection *dbuscon, const std::string &devpath)
    : DBusProxy(dbuscon,
                OpenVPN3DBus_name_netcfg,
                OpenVPN3DBus_interf_netcfg,
                devpath)
{
}


void Device::SetRemoteAddress(const std::string &remote, bool ipv6)
{
    GVariant *r = Call("SetRemoteAddress",
                       g_variant_new("(sb)",
                                     remote.c_str(),
                                     ipv6));
    g_variant_unref(r);
}


bool Device::AddBypassRoute(const std::string &addr,
                            bool ipv6)
{
    GVariant *r = Call("AddBypassRoute", g_variant_new("(sb)", addr.c_str(), ipv6));
    g_variant_unref(r);
    // TODO: handle return value
    return true;
}


void Device::AddIPAddress(const std::string &ip_address,
                          const unsigned int prefix,
                          const std::string &gateway,
                          bool ipv6)
{
    GVariant *r = Call("AddIPAddress",
                       g_variant_new("(susb)",
                                     ip_address.c_str(),
                                     prefix,
                                     gateway.c_str(),
                                     ipv6));
    g_variant_unref(r);
}


void Device::AddNetworks(const std::vector<Network> &networks)
{
    GVariantBuilder *bld = g_variant_builder_new(G_VARIANT_TYPE("a(subb)"));
    for (const auto &net : networks)
    {
        g_variant_builder_add(bld,
                              "(subb)",
                              net.address.c_str(),
                              net.prefix,
                              net.ipv6,
                              net.exclude);
    }

    // DBus somehow wants this still wrapped being able to do this
    // with one builder would be to simple or not broken enough
    GVariant *res = Call("AddNetworks", GLibUtils::wrapInTuple(bld));

    g_variant_unref(res);
}


void Device::AddDNS(const std::vector<std::string> &server_list)
{
    GVariant *list = GLibUtils::GVariantTupleFromVector(server_list);
    GVariant *res = Call("AddDNS", list);
    g_variant_unref(res);
}


void Device::RemoveDNS(const std::vector<std::string> &server_list)
{
    GVariant *list = GLibUtils::GVariantTupleFromVector(server_list);
    GVariant *res = Call("RemoveDNS", list);
    g_variant_unref(res);
}


void Device::AddDNSSearch(const std::vector<std::string> &domains)
{
    GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
    GVariant *res = Call("AddDNSSearch", list);
    g_variant_unref(res);
}


void Device::RemoveDNSSearch(const std::vector<std::string> &domains)
{
    GVariant *list = GLibUtils::GVariantTupleFromVector(domains);
    GVariant *res = Call("RemoveDNSSearch", list);
    g_variant_unref(res);
}


#ifdef ENABLE_OVPNDCO
DCO *Device::EnableDCO(const std::string &dev_name)
{
    GVariant *res = Call("EnableDCO",
                         g_variant_new("(s)", dev_name.c_str()));
    gchar *path = nullptr;
    g_variant_get(res, "(o)", &path);
    std::string dcopath{path};
    g_free(path);
    g_variant_unref(res);
    return new DCO(GetConnection(), dcopath);
}


void Device::EstablishDCO()
{
    // same as Device::Establish() but without return value
    GVariant *res = Call("Establish");
    g_variant_unref(res);
}
#endif // ENABLE_OVPNDCO


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
    return (NetCfgDeviceType)GetUIntProperty("layer");
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



#ifdef ENABLE_OVPNDCO
DCO::DCO(GDBusConnection *dbuscon, const std::string &dcopath)
    : DBusProxy(dbuscon,
                OpenVPN3DBus_name_netcfg,
                OpenVPN3DBus_interf_netcfg,
                dcopath)
{
}


int DCO::GetPipeFD()
{
    gint fd = -1;
    GVariant *res = CallGetFD("GetPipeFD", fd);
    g_variant_unref(res);
    return fd;
}


void DCO::NewPeer(unsigned int peer_id,
                  int transport_fd,
                  const sockaddr *sa,
                  unsigned int salen,
                  const IPv4::Addr &vpn4,
                  const IPv6::Addr &vpn6)
{
    auto sa_str = base64->encode(sa, salen);

    CallSendFD("NewPeer",
               g_variant_new("(ususs)",
                             peer_id,
                             sa_str.c_str(),
                             salen,
                             vpn4.to_string().c_str(),
                             vpn6.to_string().c_str()),
               transport_fd);
}


void DCO::NewKey(unsigned int key_slot, const KoRekey::KeyConfig *kc_arg)
{
    DcoKeyConfig kc;

    kc.set_key_id(kc_arg->key_id);
    kc.set_remote_peer_id(kc_arg->remote_peer_id);
    kc.set_cipher_alg(kc_arg->cipher_alg);

    auto copyKeyDirection = [](const KoRekey::KeyDirection &src, DcoKeyConfig_KeyDirection *dst)
    {
        dst->set_cipher_key(src.cipher_key, src.cipher_key_size);
        dst->set_nonce_tail(src.nonce_tail, sizeof(src.nonce_tail));
        dst->set_cipher_key_size(src.cipher_key_size);
    };

    copyKeyDirection(kc_arg->encrypt, kc.mutable_encrypt());
    copyKeyDirection(kc_arg->decrypt, kc.mutable_decrypt());

    auto str = base64->encode(kc.SerializeAsString());

    Call("NewKey", g_variant_new("(us)", key_slot, str.c_str()));
}


void DCO::SwapKeys(unsigned int peer_id)
{
    GVariant *res = Call("SwapKeys", g_variant_new("(u)", peer_id));
    g_variant_unref(res);
}


void DCO::SetPeer(unsigned int peer_id, int keepalive_interval, int keepalive_timeout)
{
    GVariant *res = Call("SetPeer",
                         g_variant_new("(uuu)",
                                       peer_id,
                                       keepalive_interval,
                                       keepalive_timeout));
    g_variant_unref(res);
}
#endif // ENABLE_OVPNDCO
} // namespace NetCfgProxy
