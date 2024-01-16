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
 * @file   proxy-netcfg-device.cpp
 *
 * @brief  Implementation of D-Bus proxy for the device objects
 *         of the net.openvpn.v3.netcfg service
 */

#include "build-config.h"

#include <string>
#include <vector>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/proxy.hpp>

#define OPENVPN_EXTERN extern
#include <openvpn/common/rc.hpp>
#include <openvpn/common/base64.hpp>
#include <openvpn/log/logbase.hpp>

#include "dbus/constants.hpp"
#include "proxy-netcfg-device.hpp"
// #include "netcfg-device.hpp"
#include "netcfg-exception.hpp"
#ifdef ENABLE_OVPNDCO
#include "dco/dco-keyconfig.pb.h"

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

Device::Device(DBus::Connection::Ptr dbuscon_, const std::string &devpath)
    : dbuscon(dbuscon_),
      proxy(DBus::Proxy::Client::Create(dbuscon, Constants::GenServiceName("netcfg"))),
      prxtgt(DBus::Proxy::TargetPreset::Create(devpath, Constants::GenInterface("netcfg")))
{
}


void Device::SetRemoteAddress(const std::string &remote, bool ipv6)
{
    GVariant *r = proxy->Call(prxtgt,
                              "SetRemoteAddress",
                              g_variant_new("(sb)", remote.c_str(), ipv6));
    g_variant_unref(r);
}


bool Device::AddBypassRoute(const std::string &addr,
                            bool ipv6)
{
    GVariant *r = proxy->Call(prxtgt,
                              "AddBypassRoute",
                              g_variant_new("(sb)", addr.c_str(), ipv6));
    g_variant_unref(r);
    // TODO: handle return value
    return true;
}


void Device::AddIPAddress(const std::string &ip_address,
                          const unsigned int prefix,
                          const std::string &gateway,
                          bool ipv6)
{
    GVariant *r = proxy->Call(prxtgt,
                              "AddIPAddress",
                              g_variant_new("(susb)",
                                            ip_address.c_str(),
                                            prefix,
                                            gateway.c_str(),
                                            ipv6));
    g_variant_unref(r);
}


void Device::AddNetworks(const std::vector<Network> &networks)
{
    GVariantBuilder *bld = glib2::Builder::Create("a(subb)");
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
    GVariant *res = proxy->Call(prxtgt,
                                "AddNetworks",
                                glib2::Builder::FinishWrapped(bld));

    g_variant_unref(res);
}


void Device::SetDNSscope(const std::string &scope) const
{
    proxy->SetProperty(prxtgt, "dns_scope", scope);
}


void Device::AddDNS(const std::vector<std::string> &server_list)
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(server_list);
    GVariant *res = proxy->Call(prxtgt, "AddDNS", list);
    g_variant_unref(res);
}


void Device::RemoveDNS(const std::vector<std::string> &server_list)
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(server_list);
    GVariant *res = proxy->Call(prxtgt, "RemoveDNS", list);
    g_variant_unref(res);
}


void Device::AddDNSSearch(const std::vector<std::string> &domains)
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(domains);
    GVariant *res = proxy->Call(prxtgt, "AddDNSSearch", list);
    g_variant_unref(res);
}


void Device::RemoveDNSSearch(const std::vector<std::string> &domains)
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(domains);
    GVariant *res = proxy->Call(prxtgt, "RemoveDNSSearch", list);
    g_variant_unref(res);
}


#ifdef ENABLE_OVPNDCO
DCO *Device::EnableDCO(const std::string &dev_name)
{
    GVariant *res = proxy->Call(prxtgt,
                                "EnableDCO",
                                glib2::Value::CreateTupleWrapped(dev_name));
    std::string dcopath = glib2::Value::Extract<std::string>(res, 0);
    g_variant_unref(res);
    return new DCO(proxy, dcopath);
}


void Device::EstablishDCO()
{
    // same as Device::Establish() but without return value
    GVariant *res = proxy->Call(prxtgt, "Establish");
    g_variant_unref(res);
}
#endif // ENABLE_OVPNDCO


int Device::Establish()
{
    gint fd = -1;
    GVariant *res = proxy->GetFD(fd, prxtgt, "Establish", nullptr);
    g_variant_unref(res);
    return fd;
}


void Device::Disable()
{
    GVariant *res = proxy->Call(prxtgt, "Disable");
    g_variant_unref(res);
}


void Device::Destroy()
{
    GVariant *res = proxy->Call(prxtgt, "Destroy");
    g_variant_unref(res);
}


unsigned int Device::GetLogLevel()
{
    return proxy->GetProperty<uint32_t>(prxtgt, "log_level");
}


void Device::SetLogLevel(unsigned int lvl)
{
    proxy->SetProperty(prxtgt, "log_level", lvl);
}


void Device::SetLayer(unsigned int layer)
{
    proxy->SetProperty(prxtgt, "layer", layer);
}


void Device::SetMtu(unsigned int mtu)
{
    proxy->SetProperty(prxtgt, "mtu", mtu);
}


uid_t Device::GetOwner()
{
    return proxy->GetProperty<uid_t>(prxtgt, "owner");
}


void Device::SetRerouteGw(bool ipv6, bool value)
{
    if (ipv6)
    {
        proxy->SetProperty(prxtgt, "reroute_ipv6", value);
    }
    else
    {
        proxy->SetProperty(prxtgt, "reroute_ipv4", value);
    }
}


std::vector<uid_t> Device::GetACL()
{
    std::vector<uid_t> ret = proxy->GetPropertyArray<uid_t>(prxtgt, "acl");
    return ret;
}


NetCfgDeviceType Device::GetDeviceType() const
{
    return static_cast<NetCfgDeviceType>(proxy->GetProperty<uint32_t>(prxtgt, "layer"));
}


const std::string Device::GetDeviceName() const
{
    return proxy->GetProperty<std::string>(prxtgt, "device_name");
}

const std::string Device::GetDevicePath() const
{
    return prxtgt->object_path;
}


bool Device::GetActive() const
{
    return proxy->GetProperty<bool>(prxtgt, "active");
}


std::vector<std::string> Device::GetIPv4Addresses()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv4Routes()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv6Addresses()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv6Routes()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetDNS()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetDNSSearch()
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}



#ifdef ENABLE_OVPNDCO
DCO::DCO(DBus::Proxy::Client::Ptr proxy_, const std::string &dcopath)
    : proxy(proxy_),
      dcotgt(DBus::Proxy::TargetPreset::Create(dcopath, Constants::GenInterface("netcfg")))
{
}


int DCO::GetPipeFD()
{
    gint fd = -1;
    GVariant *res = proxy->GetFD(fd, dcotgt, "GetPipeFD", nullptr);
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

    proxy->SendFD(dcotgt,
                  "NewPeer",
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

    proxy->Call(dcotgt,
                "NewKey",
                g_variant_new("(us)", key_slot, str.c_str()));
}


void DCO::SwapKeys(unsigned int peer_id)
{
    GVariant *res = proxy->Call(dcotgt,
                                "SwapKeys",
                                g_variant_new("(u)", peer_id));
    g_variant_unref(res);
}


void DCO::SetPeer(unsigned int peer_id, int keepalive_interval, int keepalive_timeout)
{
    GVariant *res = proxy->Call(dcotgt,
                                "SetPeer",
                                g_variant_new("(uuu)",
                                              peer_id,
                                              keepalive_interval,
                                              keepalive_timeout));
    g_variant_unref(res);
}
#endif // ENABLE_OVPNDCO
} // namespace NetCfgProxy
