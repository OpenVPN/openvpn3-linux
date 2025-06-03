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

Device::Device(DBus::Connection::Ptr dbuscon_, const DBus::Object::Path &devpath)
    : dbuscon(dbuscon_),
      proxy(DBus::Proxy::Client::Create(dbuscon, Constants::GenServiceName("netcfg"))),
      prxtgt(DBus::Proxy::TargetPreset::Create(devpath, Constants::GenInterface("netcfg")))
{
}


void Device::SetRemoteAddress(const std::string &remote, bool ipv6) const
{
    GVariant *r = proxy->Call(prxtgt,
                              "SetRemoteAddress",
                              g_variant_new("(sb)", remote.c_str(), ipv6));
    if (r)
    {
        g_variant_unref(r);
    }
}


bool Device::AddBypassRoute(const std::string &addr,
                            bool ipv6) const
{
    GVariant *r = proxy->Call(prxtgt,
                              "AddBypassRoute",
                              g_variant_new("(sb)", addr.c_str(), ipv6));
    if (r)
    {
        g_variant_unref(r);
    }
    // TODO: handle return value
    return true;
}


void Device::AddIPAddress(const std::string &ip_address,
                          const unsigned int prefix,
                          const std::string &gateway,
                          bool ipv6) const
{
    GVariant *r = proxy->Call(prxtgt,
                              "AddIPAddress",
                              g_variant_new("(susb)",
                                            ip_address.c_str(),
                                            prefix,
                                            gateway.c_str(),
                                            ipv6));
    if (r)
    {
        g_variant_unref(r);
    }
}


void Device::AddNetworks(const std::vector<Network> &networks) const
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


    if (res)
    {
        g_variant_unref(res);
    }
}


void Device::SetDNSscope(const std::string &scope) const
{
    proxy->SetProperty(prxtgt, "dns_scope", scope);
}


bool Device::AddDnsOptions(LogSender::Ptr log, const openvpn::DnsOptions &dns) const
{
    try
    {
        // Parse --dns search-domains DOMAIN...
        std::vector<std::string> search_domains{};
        for (const auto &dns_srch : dns.search_domains)
        {
            search_domains.push_back(dns_srch.to_string());
        }

        bool split_dns = false;
        std::vector<std::string> dns_servers{};
        for (const auto &[idx, dnsopts] : dns.servers)
        {
            // Parse --dns search-domain DOMAIN...
            if (dnsopts.domains.size() > 0)
            {
                for (const auto &dns_srch : dnsopts.domains)
                {
                    search_domains.push_back(dns_srch.to_string());
                }
                split_dns = true;
            }

            // Parse --dns server X dnssec SECURITY
            if (openvpn::DnsServer::Security::Unset != dnsopts.dnssec)
            {
                try
                {
                    SetDNSSEC(dnsopts.dnssec);
                    log->LogInfo("DNS resolver setup: DNSSEC set to "
                                 + dnsopts.dnssec_string(dnsopts.dnssec));
                }
                catch (const DBus::Exception &excp)
                {
                    log->LogWarn("DNS resolver setup: "
                                 "Could not set DNSSEC mode: "
                                 + std::string(excp.GetRawError()));
                }
            }

            // Parse --dns server X transport TRANSSPORT
            if (openvpn::DnsServer::Transport::Unset != dnsopts.transport)
            {
                try
                {
                    SetDNSTransport(dnsopts.transport);
                }
                catch (const DBus::Exception &excp)
                {
                    log->LogError("DNS resolver setup: Failed setting DNS transport to "
                                  + dnsopts.transport_string(dnsopts.transport));
                    log->Debug("DNS transport setting error: "
                               + std::string(excp.GetRawError()));
                    return false;
                }
            }

            // Parse --dns server X address RESOLVER_ADDR...
            for (const auto &dns_addr : dnsopts.addresses)
            {
                if ("UNSPEC" == dns_addr.address)
                {
                    log->LogError("DNS resolver setup: "
                                  "Ignoring incorrect DNS server address: "
                                  + dns_addr.to_string());
                    continue;
                }

                if (dns_addr.port != 0 && dns_addr.port != 53)
                {
                    log->LogWarn("DNS resolver setup: "
                                 "DNS server port not supported, ignoring "
                                 + dns_addr.to_string());
                    continue;
                }
                dns_servers.push_back(dns_addr.address);
            }
        }

        // Send the parsed DNS resolver settings to the netcfg service
        AddDNSSearch(search_domains);
        AddDNS(dns_servers);
        if (split_dns)
        {
            log->LogInfo("Changing DNS scope to 'tunnel'");
            SetDNSscope("tunnel");
        }
        return true;
    }
    catch (const std::exception &excp)
    {
        log->LogError("Failed to parse --dns options: "
                      + std::string(excp.what()));
        return false;
    }
}


void Device::AddDNS(const std::vector<std::string> &server_list) const
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(server_list);
    GVariant *res = proxy->Call(prxtgt, "AddDNS", list);
    if (res)
    {
        g_variant_unref(res);
    }
}



void Device::AddDNSSearch(const std::vector<std::string> &domains) const
{
    GVariant *list = glib2::Value::CreateTupleWrapped<std::string>(domains);
    GVariant *res = proxy->Call(prxtgt, "AddDNSSearch", list);
    if (res)
    {
        g_variant_unref(res);
    }
}


void Device::SetDNSSEC(const openvpn::DnsServer::Security &mode) const
{
    std::string mode_str;
    switch (mode)
    {
    case openvpn::DnsServer::Security::Yes:
        mode_str = "yes";
        break;

    case openvpn::DnsServer::Security::No:
        mode_str = "no";
        break;

    case openvpn::DnsServer::Security::Optional:
        mode_str = "optional";
        break;

    default:
        throw NetCfgProxyException("SetDNSSEC", "Invalid DNSSEC mode");
    }

    GVariant *res = proxy->Call(prxtgt,
                                "SetDNSSEC",
                                glib2::Value::CreateTupleWrapped(mode_str));
    if (res)
    {
        g_variant_unref(res);
    }
}


openvpn::DnsServer::Security Device::GetDNSSEC() const
{
    auto mode = proxy->GetProperty<std::string>(prxtgt, "dnssec_mode");
    if ("yes" == mode)
    {
        return openvpn::DnsServer::Security::Yes;
    }
    else if ("no" == mode)
    {
        return openvpn::DnsServer::Security::No;
    }
    else if ("optional" == mode)
    {
        return openvpn::DnsServer::Security::Optional;
    }
    else if ("unset" == mode)
    {
        return openvpn::DnsServer::Security::Unset;
    }
    throw NetCfgProxyException("GetDNSSEC", "Invalid DNSSEC mode");
}


void Device::SetDNSTransport(const openvpn::DnsServer::Transport &mode) const
{
    std::string mode_str;
    switch (mode)
    {
    case openvpn::DnsServer::Transport::Plain:
        mode_str = "plain";
        break;

    case openvpn::DnsServer::Transport::TLS:
        mode_str = "dot";
        break;

    case openvpn::DnsServer::Transport::HTTPS:
        throw NetCfgProxyException("SetDNSTransport", "DNS-over-HTTPS is not supported");

    case openvpn::DnsServer::Transport::Unset:
        mode_str = "unset";
        break;

    default:
        throw NetCfgProxyException("SetDNSTransport", "Invalid DNS transport mode");
    }
    GVariant *res = proxy->Call(prxtgt,
                                "SetDNSTransport",
                                glib2::Value::CreateTupleWrapped(mode_str));
    if (res)
    {
        g_variant_unref(res);
    }
}


openvpn::DnsServer::Transport Device::GetDNSTransport() const
{
    auto mode = proxy->GetProperty<std::string>(prxtgt, "dns_transport");
    if ("plain" == mode)
    {
        return openvpn::DnsServer::Transport::Plain;
    }
    else if ("dot" == mode)
    {
        return openvpn::DnsServer::Transport::TLS;
    }
    else if ("doh" == mode)
    {
        throw NetCfgProxyException("SetDNSTransport", "DNS-over-HTTPS is not supported");
    }
    else if ("unset" == mode)
    {
        return openvpn::DnsServer::Transport::Unset;
    }
    throw NetCfgProxyException("GetDNSTransport", "Invalid DNS transport mode");
}


#ifdef ENABLE_OVPNDCO
DCO *Device::EnableDCO(const std::string &dev_name) const
{
    GVariant *res = proxy->Call(prxtgt,
                                "EnableDCO",
                                glib2::Value::CreateTupleWrapped(dev_name));
    auto dcopath = glib2::Value::Extract<DBus::Object::Path>(res, 0);
    if (res)
    {
        g_variant_unref(res);
    }
    return new DCO(proxy, dcopath);
}


void Device::EstablishDCO() const
{
    // same as Device::Establish() but without return value
    GVariant *res = proxy->Call(prxtgt, "Establish");
    if (res)
    {
        g_variant_unref(res);
    }
}
#endif // ENABLE_OVPNDCO


int Device::Establish() const
{
    int fd = -1;
    GVariant *res = proxy->GetFD(fd, prxtgt, "Establish", nullptr);
    if (res)
    {
        g_variant_unref(res);
    }
    return fd;
}


void Device::Disable() const
{
    GVariant *res = proxy->Call(prxtgt, "Disable");
    if (res)
    {
        g_variant_unref(res);
    }
}


void Device::Destroy() const
{
    GVariant *res = proxy->Call(prxtgt, "Destroy");
    if (res)
    {
        g_variant_unref(res);
    }
}


unsigned int Device::GetLogLevel() const
{
    return proxy->GetProperty<uint32_t>(prxtgt, "log_level");
}


void Device::SetLogLevel(unsigned int lvl) const
{
    proxy->SetProperty(prxtgt, "log_level", lvl);
}


void Device::SetLayer(unsigned int layer) const
{
    proxy->SetProperty(prxtgt, "layer", layer);
}


void Device::SetMtu(const uint16_t mtu) const
{
    proxy->SetProperty(prxtgt, "mtu", mtu);
}


uid_t Device::GetOwner() const
{
    return proxy->GetProperty<uid_t>(prxtgt, "owner");
}


void Device::SetRerouteGw(bool ipv6, bool value) const
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


std::vector<uid_t> Device::GetACL() const
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

const DBus::Object::Path Device::GetDevicePath() const
{
    return prxtgt->object_path;
}


bool Device::GetActive() const
{
    return proxy->GetProperty<bool>(prxtgt, "active");
}


std::vector<std::string> Device::GetIPv4Addresses() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv4Routes() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv6Addresses() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetIPv6Routes() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetDNS() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}


std::vector<std::string> Device::GetDNSSearch() const
{
    // FIXME:
    std::vector<std::string> ret;
    return ret;
}



#ifdef ENABLE_OVPNDCO
DCO::DCO(DBus::Proxy::Client::Ptr proxy_, const DBus::Object::Path &dcopath)
    : proxy(proxy_),
      dcotgt(DBus::Proxy::TargetPreset::Create(dcopath, Constants::GenInterface("netcfg")))
{
}


int DCO::GetPipeFD()
{
    gint fd = -1;
    GVariant *res = proxy->GetFD(fd, dcotgt, "GetPipeFD", nullptr);
    if (res)
    {
        g_variant_unref(res);
    }
    return fd;
}


void DCO::NewPeer(unsigned int peer_id,
                  int transport_fd,
                  const sockaddr *sa,
                  unsigned int salen,
                  const IPv4::Addr &vpn4,
                  const IPv6::Addr &vpn6) const
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


void DCO::NewKey(unsigned int key_slot,
                 const KoRekey::KeyConfig *kc_arg) const
{
    DcoKeyConfig kc;

    kc.set_key_id(kc_arg->key_id);
    kc.set_remote_peer_id(kc_arg->remote_peer_id);
    kc.set_cipher_alg(kc_arg->cipher_alg);

    auto copyKeyDirection = [](const KoRekey::KeyDirection &src,
                               DcoKeyConfig_KeyDirection *dst)
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


void DCO::SwapKeys(unsigned int peer_id) const
{
    GVariant *res = proxy->Call(dcotgt,
                                "SwapKeys",
                                g_variant_new("(u)", peer_id));
    if (res)
    {
        g_variant_unref(res);
    }
}


void DCO::SetPeer(unsigned int peer_id,
                  int keepalive_interval,
                  int keepalive_timeout) const
{
    GVariant *res = proxy->Call(dcotgt,
                                "SetPeer",
                                g_variant_new("(uuu)",
                                              peer_id,
                                              keepalive_interval,
                                              keepalive_timeout));
    if (res)
    {
        g_variant_unref(res);
    }
}
#endif // ENABLE_OVPNDCO
} // namespace NetCfgProxy
