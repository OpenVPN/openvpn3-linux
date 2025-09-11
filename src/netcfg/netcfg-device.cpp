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

#include <fmt/format.h>

#include "common/string-utils.hpp"
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
                           LogWriter *logwr_,
                           const NetCfgOptions &options)
    : DBus::Object::Base(objpath, Constants::GenInterface("netcfg")),
      dbuscon(dbuscon_),
      object_manager(obj_mgr),
      device_name(devname),
      object_acl(GDBusPP::Object::Extension::ACL::Create(dbuscon_, creator_)),
      creator_pid(creator_pid_),
      resolver(resolver),
      logwr(logwr_),
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

    if (resolver)
    {
        dnsconfig = resolver->NewResolverSettings();
    }

    AddProperty("device_name", device_name, false);
    AddProperty("mtu", mtu, true);
    AddProperty("layer", device_type, true);
    AddProperty("txqueuelen", txqueuelen, true);
    AddProperty("reroute_ipv4", reroute_ipv4, true);
    AddProperty("reroute_ipv6", reroute_ipv6, true);

    AddPropertyBySpec(
        "owner",
        glib2::DataType::DBus<uint32_t>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::Create(object_acl->GetOwner());
        });

    AddPropertyBySpec(
        "acl",
        "au",
        [this](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return glib2::Value::CreateVector(object_acl->GetAccessList());
        });

    AddPropertyBySpec(
        "dns_scope",
        glib2::DataType::DBus<std::string>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            std::string scope = (dnsconfig
                                     ? dnsconfig->GetDNSScopeStr()
                                     : "");
            return glib2::Value::Create(scope);
        },
        [&](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
        {
            if (!dnsconfig)
            {
                throw DBus::Object::Property::Exception(
                    this, "dns_scope", "No DNS resolver configured");
            }
            std::string scope = dnsconfig->SetDNSScope(value);
            signals->DebugDevice(device_name,
                                 "Changed DNS resolver scope to '"
                                     + scope + "'");
            auto upd = prop.PrepareUpdate();
            upd->AddValue(scope);
            return upd;
        });

    AddPropertyBySpec(
        "dns_name_servers",
        glib2::DataType::DBus<std::string>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return (dnsconfig
                        ? glib2::Value::CreateVector(dnsconfig->GetNameServers())
                        : glib2::Value::CreateVector(std::vector<std::string>{}));
        });

    AddPropertyBySpec(
        "dns_search_domains",
        glib2::DataType::DBus<std::string>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            return (dnsconfig
                        ? glib2::Value::CreateVector(dnsconfig->GetSearchDomains())
                        : glib2::Value::CreateVector(std::vector<std::string>{}));
        });

    AddPropertyBySpec(
        "dnssec_mode",
        glib2::DataType::DBus<std::string>(),
        [&](const DBus::Object::Property::BySpec &prop) -> GVariant *
        {
            std::string mode = (dnsconfig
                                    ? dnsconfig->GetDNSSEC_string()
                                    : "");
            return glib2::Value::Create(mode);
        });


    auto args_add_ipaddr = AddMethod(
        "AddIPAddress",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_add_ip_address(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_add_ipaddr->AddInput("ip_address", "s");
    args_add_ipaddr->AddInput("prefix_size", glib2::DataType::DBus<unsigned int>());
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
    args_add_networks->AddInput("networks", "a(suibb)");


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

    auto args_set_dnssec = AddMethod(
        "SetDNSSEC",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_set_dnssec(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_set_dnssec->AddInput("mode", "s");

    auto args_set_dnstransp = AddMethod(
        "SetDNSTransport",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_set_dns_transport(args->GetMethodParameters());
            args->SetMethodReturn(nullptr);
        });
    args_set_dnstransp->AddInput("mode", "s");

#ifdef ENABLE_OVPNDCO
    auto args_enable_dco = AddMethod(
        "EnableDCO",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_enable_dco(args);
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
        object_manager->RemoveObject(dco_device->GetPath());
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

    std::string ipaddr = filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 0), true);
    uint32_t prefix_size = glib2::Value::Extract<uint32_t>(params, 1);
    std::string gateway = filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 2), true);
    bool ipv6 = glib2::Value::Extract<bool>(params, 3);

    signals->LogInfo(std::string("Adding IP Address ") + ipaddr
                     + "/" + std::to_string(prefix_size)
                     + " gw " + gateway + " ipv6: " + (ipv6 ? "yes" : "no"));

    vpnips.emplace_back(VPNAddress(std::string(ipaddr), prefix_size, std::string(gateway), ipv6));
}


void NetCfgDevice::method_set_remote_addr(GVariant *params)
{

    glib2::Utils::checkParams(__func__, params, "(sb)", 2);

    std::string ipaddr{filter_ctrl_chars(glib2::Value::Extract<std::string>(params, 0), true)};
    bool ipv6{glib2::Value::Extract<bool>(params, 1)};

    signals->LogInfo(std::string("Setting remote IP address to ")
                     + ipaddr + " ipv6: " + (ipv6 ? "yes" : "no"));
    remote = IPAddr(std::string(ipaddr), ipv6);
}


void NetCfgDevice::method_add_networks(GVariant *params)
{
    /*
     *  D-Bus Data type description
     *
     *  The argument given contains an array of networks to apply
     *
     *  s  -  string, contains the IPv4/IPv6 network address (without prefix length)
     *  u  -  uint32_t, containing the network prefix length
     *  i  -  int32_t, route metric value; if '-1', the Core library will use the default metric
     *  b  -  bool, IPv6 address flag
     *  b  -  bool, exclude flag.  Route is to be excluded if true
     */
    glib2::Utils::checkParams(__func__, params, "(a(suibb))", 1);
    GVariantIter *network_iter;
    g_variant_get(params, "(a(suibb))", &network_iter);

    GVariant *network_descr = nullptr;
    while ((network_descr = g_variant_iter_next_value(network_iter)))
    {
        try
        {
            glib2::Utils::checkParams(__func__, network_descr, "(suibb)", 5);
        }
        catch (const DBus::Exception &excp)
        {
            char *data = g_variant_print(network_descr, true);
            std::string err = fmt::format("{} - Data: {}",
                                          excp.GetRawError(),
                                          std::string(data));
            free(data);
            throw NetCfgException(err);
        }

        // FIXME: migrate into Network class
        auto netw_addr{filter_ctrl_chars(glib2::Value::Extract<std::string>(network_descr, 0), true)};
        auto prefix_size{glib2::Value::Extract<uint32_t>(network_descr, 1)};
        auto metric{glib2::Value::Extract<int32_t>(network_descr, 2)};
        auto ipv6{glib2::Value::Extract<bool>(network_descr, 3)};
        auto exclude{glib2::Value::Extract<bool>(network_descr, 4)};

        std::string metric_str = (metric > 0 ? std::to_string(metric) : "(default)");
        signals->LogInfo(fmt::format(
            "Adding network {}/{}, metric: {}, exclude: {}, ipv6: {}",
            netw_addr,
            prefix_size,
            metric_str,
            (exclude ? "yes" : "no"),
            (ipv6 ? "yes" : "no")));

        networks.emplace_back(netw_addr, prefix_size, metric, ipv6, exclude);
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
        return;
    }

    // Adds DNS search domains
    dnsconfig->AddSearchDomains(params);
    modified = true;
}


void NetCfgDevice::method_set_dnssec(GVariant *params)
{
    if (!resolver || !dnsconfig)
    {
        signals->LogError("Failed setting DNSSEC mode: "
                          "No DNS resolver configured");
        return;
    }
    dnsconfig->SetDNSSEC(params);
    modified = true;
}


void NetCfgDevice::method_set_dns_transport(GVariant *params)
{
    if (!resolver || !dnsconfig)
    {
        signals->LogError("Failed setting DNS transport mode: "
                          "No DNS resolver configured");
        return;
    }
    dnsconfig->SetDNSTransport(params);
    modified = true;
}


#ifdef ENABLE_OVPNDCO
void NetCfgDevice::method_enable_dco(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    glib2::Utils::checkParams(__func__, params, "(s)", 1);
    std::string dev_name = filter_ctrl_chars(
        glib2::Value::Extract<std::string>(params, 0),
        true);
    set_device_name(dev_name);

    try
    {
        dco_device = object_manager->CreateObject<NetCfgDCO>(
            dbuscon,
            GetPath(),
            dev_name,
            creator_pid,
            logwr);
    }
    catch (const NetCfgException &excp)
    {
        throw DBus::Object::Method::Exception(excp.what());
    }

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
        signals->LogCritical(
            fmt::format("Failed to setup a TUN interface: {}",
                        excp.what()));
    }
    catch (const std::exception &excp)
    {
        signals->LogCritical(
            fmt::format("Failed to setup TUN interface (Core Exception): {}",
                        excp.what()));
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

    std::string caller = args->GetCallerBusName();

    watcher = std::make_unique<DBus::BusWatcher>(dbuscon->GetBusType(), caller);
    watcher->SetNameDisappearedHandler(
        [this](const std::string &bus_name)
        {
            signals->LogError("Client process with bus name " + bus_name + " disappeared! Cleaning up.");
            destroy();
        });

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


void NetCfgDevice::destroy()
{
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

    // release the NetCfgDevice object from memory as well, which
    // will should do the proper interface teardown calls in the
    // destructor
    object_manager->RemoveObject(GetPath());
}


void NetCfgDevice::method_destroy(DBus::Object::Method::Arguments::Ptr args)
{
    auto credsq = DBus::Credentials::Query::Create(dbuscon);
    std::string caller = args->GetCallerBusName();
    uid_t caller_uid = credsq->GetUID(caller);
    // CheckOwnerAccess(caller);

    std::string sender_name = lookup_username(caller_uid);

    destroy();

    signals->LogVerb1("Device '" + device_name + "' was removed by "
                      + sender_name);
}
