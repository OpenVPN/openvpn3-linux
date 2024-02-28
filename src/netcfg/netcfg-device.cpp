//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2019-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2021-  Antonio Quartulli <antonio@openvpn.net>
//

/**
 * @file   netcfg-device.cpp
 *
 * @brief  D-Bus object representing a single virtual network device
 *         the net.openvpn.v3.netcfg service manages
 */

#include "build-config.h"

#include "netcfg-device.hpp"

#ifdef ENABLE_OVPNDCO
#include "netcfg-dco.hpp"
#endif

using namespace openvpn;
using namespace NetCfg;


NetCfgDevice::NetCfgDevice(DBus::Connection::Ptr dbuscon_,
                           DBus::Object::Manager::Ptr obj_mgr,
                           const uid_t creator_,
                           const pid_t creator_pid_,
                           const std::string &objpath,
                           const std::string &devname,
                           DNS::SettingsManager::Ptr resolver,
                           NetCfgSubscriptions::Ptr subscriptions,
                           const unsigned int log_level,
                           LogWriter *logwr,
                           const NetCfgOptions &options)
    : DBus::Object::Base(objpath, Constants::GenInterface("netcfg")),
      dbuscon(dbuscon_),
      object_manager(obj_mgr),
      device_name(devname), creator_pid(creator_pid_),
      resolver(resolver),
      options(std::move(options))
{
    signals = NetCfgSignals::Create(dbuscon_,
                                    LogGroup::NETCFG,
                                    objpath,
                                    logwr);
    signals->SetLogLevel(log_level);

    if (subscriptions)
    {
        signals->AddSubscriptionList(subscriptions);
    }

    AddProperty("device_name", device_name, false);
    AddProperty("mtu", mtu, true);
    AddProperty("layer", device_type, true);
    AddProperty("txqueuelen", txqueuelen, true);
    AddProperty("reroute_ipv4", reroute_ipv4, true);
    AddProperty("reroute_ipv6", reroute_ipv6, true);

    auto args_add_ipaddr = AddMethod(
        "AddIPAddress",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_add_ip_address(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_add_ipaddr->AddInput("ip_address", "s");
    args_add_ipaddr->AddInput("prefix", glib2::DataType::DBus<unsigned int>());
    args_add_ipaddr->AddInput("gateway", "s");
    args_add_ipaddr->AddInput("ipv6", glib2::DataType::DBus<bool>());

    auto args_set_remote_addr = AddMethod(
        "SetRemoteAddress",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_set_remote_addr(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_set_remote_addr->AddInput("ip_address", "s");
    args_set_remote_addr->AddInput("ipv6", glib2::DataType::DBus<bool>());


    auto args_add_networks = AddMethod(
        "AddNetworks",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_add_networks(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_add_networks->AddInput("networks", "a(subb)");


    auto args_add_dns = AddMethod(
        "AddDNS",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_add_dns(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_add_dns->AddInput("server_list", "as");

    auto args_add_dns_srch = AddMethod(
        "AddDNSSearch",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_add_dns_search(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_add_dns_srch->AddInput("domains", "as");

#ifdef ENABLE_OVPNDCO
    auto args_enable_dco = AddMethod(
        "EnableDCO",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_enable_dco(args);
            args->SetMethodReturn(nullptr);
        });
    args_enable_dco->AddInput("dev_name", "s");
    args_enable_dco->AddOutput("dco_device_path", "o");
#endif

    auto args_establish = AddMethod(
        "Establish",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_establish(args);
        });
    args_establish->PassFileDescriptor(DBus::Object::Method::PassFDmode::SEND);

    AddMethod("Disable",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  this->method_disable();
                  args->SetMethodReturn(nullptr);
              });

    AddMethod("Destroy",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  this->method_destroy(args);
              });


    if (resolver)
    {
        dnsconfig = resolver->NewResolverSettings();
    }

    signals->LogVerb2("Network device '" + devname + "' prepared");
}


NetCfgDevice::~NetCfgDevice() noexcept
{
    if (tunimpl)
    {
        tunimpl->teardown(*this, true);
        tunimpl.reset();
    }
#ifdef ENABLE_OVPNDCO
    if (dco_device)
    {
    }
#endif
}


const bool NetCfgDevice::Authorize(const DBus::Authz::Request::Ptr request)
{
    return true;
};



std::string NetCfgDevice::get_device_name() const noexcept
{
    return device_name;
}


void NetCfgDevice::set_device_name(const std::string &devnam) noexcept
{
    signals->DebugDevice(devnam,
                         "Device name changed from '" + device_name + "'");
    device_name = devnam;
}


pid_t NetCfgDevice::getCreatorPID()
{
    return creator_pid;
}


void NetCfgDevice::method_add_ip_address(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(susb)", 4);

    std::string ipaddr = glib2::Value::Extract<std::string>(params, 0);
    uint32_t prefix = glib2::Value::Extract<uint32_t>(params, 1);
    std::string gateway = glib2::Value::Extract<std::string>(params, 2);
    bool ipv6 = glib2::Value::Extract<bool>(params, 3);

    signals->LogInfo(std::string("Adding IP Address ") + ipaddr
                     + "/" + std::to_string(prefix)
                     + " gw " + gateway + " ipv6: " + (ipv6 ? "yes" : "no"));

    vpnips.emplace_back(VPNAddress(std::string(ipaddr), prefix, std::string(gateway), ipv6));
}


void NetCfgDevice::method_set_remote_addr(GVariant *params)
{

    glib2::Utils::checkParams(__func__, params, "(sb)", 2);

    std::string ipaddr{glib2::Value::Extract<std::string>(params, 0)};
    bool ipv6{glib2::Value::Extract<bool>(params, 1)};

    signals->LogInfo(std::string("Setting remote IP address to ")
                     + ipaddr + " ipv6: " + (ipv6 ? "yes" : "no"));
    remote = IPAddr(std::string(ipaddr), ipv6);
}


void NetCfgDevice::method_add_networks(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(a(subb))", 1);
    GVariantIter *network_iter;
    g_variant_get(params, "(a(subb))", &network_iter);

    GVariant *network = nullptr;
    while ((network = g_variant_iter_next_value(network_iter)))
    {
        glib2::Utils::checkParams(__func__, network, "(subb)", 4);

        std::string net{glib2::Value::Extract<std::string>(network, 0)};
        uint32_t prefix{glib2::Value::Extract<uint32_t>(network, 1)};
        bool ipv6{glib2::Value::Extract<bool>(network, 2)};
        bool exclude{glib2::Value::Extract<bool>(network, 3)};

        signals->LogInfo(std::string("Adding network '") + net + "/"
                         + std::to_string(prefix)
                         + "' excl: " + (exclude ? "yes" : "no")
                         + " ipv6: " + (ipv6 ? "yes" : "no"));

        networks.emplace_back(Network(std::string(net), prefix, ipv6, exclude));
    }
    // FIXME:  No need to unref GVariant *network ?
    g_variant_iter_free(network_iter);
}


void NetCfgDevice::method_add_dns(GVariant *params)
{
    if (!resolver || !dnsconfig)
    {
        signals->LogError("Failed adding DNS server: "
                          "No DNS resolver configured");
        return;
    }

    // Adds DNS name servers
    std::string added = dnsconfig->AddNameServers(params);
    signals->DebugDevice(device_name, "Added DNS name servers: " + added);
    modified = true;
}


void NetCfgDevice::method_add_dns_search(GVariant *params)
{
    if (!resolver || !dnsconfig)
    {
        signals->LogError("Failed adding DNS search domains: "
                          "No DNS resolver configured");
    }

    // Adds DNS search domains
    dnsconfig->AddSearchDomains(params);
    modified = true;
}


#ifdef ENABLE_OVPNDCO
void NetCfgDevice::method_enable_dco(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    glib2::Utils::checkParams(__func__, params, "(s)", 1);
    std::string dev_name = glib2::Value::Extract<std::string>(params, 0);
    set_device_name(dev_name);

    dco_device = object_manager->CreateObject<NetCfgDCO>(
        dbuscon,
        GetPath(),
        dev_name,
        creator_pid,
        signals);

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(dco_device->GetPath()));
}
#endif

void NetCfgDevice::method_establish(DBus::Object::Method::Arguments::Ptr args)
{
    // The virtual device has not yet been created on the host (for
    // non-DCO case), but all settings which has been queued up
    // will be activated when this method is called.
    try
    {
        if (resolver && dnsconfig
            && DNS::ApplySettingsMode::MODE_PRE == resolver->GetApplyMode())
        {
            dnsconfig->Enable();
            resolver->ApplySettings(signals);
        }
    }
    catch (const NetCfgException &excp)
    {
        // FIXME: Include device name
        signals->LogCritical("DNS Resolver settings: "
                             + std::string(excp.what()));
    }

    if (!tunimpl)
    {
        tunimpl.reset(getCoreBuilderInstance());
    }

    int fd = -1;
    try
    {
        fd = tunimpl->establish(*this);
    }
    catch (const NetCfgException &excp)
    {
        signals->LogCritical("Failed to setup a TUN interface: "
                             + std::string(excp.what()));
    }

    try
    {
        if (resolver && dnsconfig)
        {
            if (DNS::ApplySettingsMode::MODE_POST == resolver->GetApplyMode())
            {
                dnsconfig->SetDeviceName(device_name);
                dnsconfig->Enable();
                resolver->ApplySettings(signals);
            }

            std::stringstream details;
            details << dnsconfig;
            signals->DebugDevice(device_name,
                                 "Activating DNS/resolver settings: "
                                     + details.str());
            modified = false;
        }
    }
    catch (const NetCfgException &excp)
    {
        signals->LogCritical("DNS Resolver settings: "
                             + std::string(excp.what()));
    }
#ifdef ENABLE_OVPNDCO
    // in DCO case don't return anything
    if (!dco_device)
    {
        args->SendFD(fd);
    }
#else  // Without DCO support compiled in, a FD to the tun device is always returned
    args->SendFD(fd);
#endif // ENABLE_OVPNDCO
    args->SetMethodReturn(nullptr);
}


void NetCfgDevice::method_disable()
{
    if (resolver && dnsconfig)
    {
        std::stringstream details;
        details << dnsconfig;

        signals->DebugDevice(device_name,
                             "Disabling DNS/resolver settings: "
                                 + details.str());

        dnsconfig->Disable();
        resolver->ApplySettings(signals);

        // We need to clear these settings, as the CoreVPNClient
        // will re-add them upon activation again.
        dnsconfig->ClearNameServers();
        dnsconfig->ClearSearchDomains();

        modified = false;
    }
    if (tunimpl)
    {
        tunimpl->teardown(*this, true);
        tunimpl.reset();
    }
}


void NetCfgDevice::method_destroy(DBus::Object::Method::Arguments::Ptr args)
{
    auto credsq = DBus::Credentials::Query::Create(dbuscon);
    std::string caller = args->GetCallerBusName();
    uid_t caller_uid = credsq->GetUID(caller);
    // CheckOwnerAccess(caller);

    if (resolver && dnsconfig)
    {
        std::stringstream details;
        details << dnsconfig;

        signals->DebugDevice(device_name,
                             "Removing DNS/resolver settings: "
                                 + details.str());
        dnsconfig->PrepareRemoval();
        resolver->ApplySettings(signals);
        modified = false;
    }

    std::string sender_name = lookup_username(caller_uid);
    signals->LogVerb1("Device '" + device_name + "' was removed by "
                      + sender_name);

    // Remove this object from the D-Bus.  This will also
    // release the NetCfgDevice object from memory as well, which
    // will should do the proper interface teardown calls in the
    // destructor
    object_manager->RemoveObject(GetPath());
    args->SetMethodReturn(nullptr);
}
