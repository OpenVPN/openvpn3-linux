//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018-  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2018-  Arne Schwabe <arne@openvpn.net>
//  Copyright (C) 2020-  Lev Stipakov <lev@openvpn.net>
//  Copyright (C) 2021-  Heiko Hund <heiko@openvpn.net>
//

/**
 * @file   netcfg-service-handler.cpp
 *
 * @brief  Implementation of the NetCfgServiceHandler object
 */

#include <gdbuspp/object/base.hpp>

#include "log/core-dbus-logger.hpp"
#include "netcfg-device.hpp"
#include "netcfg-service-handler.hpp"


NetCfgServiceHandler::NetCfgServiceHandler(DBus::Connection::Ptr conn_,
                                           const unsigned int default_log_level,
                                           DNS::SettingsManager::Ptr resolver,
                                           DBus::Object::Manager::Ptr obj_mgr,
                                           LogWriter *logwr,
                                           NetCfgOptions options)
    : DBus::Object::Base(Constants::GenPath("netcfg"),
                         Constants::GenInterface("netcfg")),
      conn(conn_),
      object_manager(obj_mgr),
      resolver(resolver),
      options(std::move(options))
{
    DisableIdleDetector(true);

    creds_query = DBus::Credentials::Query::Create(conn);

    signals = NetCfgSignals::Create(conn,
                                    LogGroup::NETCFG,
                                    Constants::GenPath("netcfg"),
                                    logwr),
    signals->SetLogLevel(default_log_level);
    RegisterSignals(signals);

    auto prop_glob_dns_srvs = [resolver](const DBus::Object::Property::BySpec &prop) -> GVariant *
    {
        if (!resolver)
        {
            // If no resolver is configured, return an empty result
            // instead of an error when reading this property
            return glib2::Value::CreateVector(std::vector<std::string>{});
        }
        return glib2::Value::CreateVector(resolver->GetDNSservers());
    };
    AddPropertyBySpec("global_dns_servers", "as", prop_glob_dns_srvs);

    auto prop_glob_dns_srch = [resolver](const DBus::Object::Property::BySpec &prop) -> GVariant *
    {
        if (!resolver)
        {
            // If no resolver is configured, return an empty result
            // instead of an error when reading this property
            return glib2::Value::CreateVector(std::vector<std::string>{});
        }
        return glib2::Value::CreateVector(resolver->GetSearchDomains());
    };
    AddPropertyBySpec("global_dns_search", "as", prop_glob_dns_srch);

    auto prop_log_level_get = [this](const DBus::Object::Property::BySpec &prop)
    {
        return glib2::Value::Create(this->signals->GetLogLevel());
    };
    auto prop_log_level_set = [this](const DBus::Object::Property::BySpec &prop, GVariant *value) -> DBus::Object::Property::Update::Ptr
    {
        try
        {
            this->signals->SetLogLevel(glib2::Value::Get<uint32_t>(value));
            auto upd = prop.PrepareUpdate();
            upd->AddValue(this->signals->GetLogLevel());
            return upd;
        }
        catch (const DBus::Exception &ex)
        {
            this->signals->LogError(ex.what());
        }
        return nullptr;
    };
    AddPropertyBySpec("log_level",
                      glib2::DataType::DBus<uint32_t>(),
                      prop_log_level_get,
                      prop_log_level_set);


    auto prop_cfg_file = [options](const DBus::Object::Property::BySpec &prop)
    {
        return glib2::Value::Create(options.config_file);
    };
    AddPropertyBySpec("config_file",
                      glib2::DataType::DBus<std::string>(),
                      prop_cfg_file);

    AddProperty("version", version, false);


    auto args_create_virt_intf = AddMethod(
        "CreateVirtualInterface",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_create_virtual_interface(args);
        });
    args_create_virt_intf->AddInput("device_name", glib2::DataType::DBus<std::string>());
    args_create_virt_intf->AddOutput("device_path", "o");

    auto args_fetch_intf_list = AddMethod(
        "FetchInterfaceList",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_fetch_interface_list(args);
        });
    args_fetch_intf_list->AddOutput("device_paths", "ao");

    auto args_protect_socket = AddMethod(
        "ProtectSocket",
        [this](DBus::Object::Method::Arguments::Ptr args)
        {
            this->method_protect_socket(args);
        });
    args_protect_socket->PassFileDescriptor(DBus::Object::Method::PassFDmode::RECEIVE);
    args_protect_socket->AddInput("remote", glib2::DataType::DBus<std::string>());
    args_protect_socket->AddInput("ipv6", glib2::DataType::DBus<bool>());
    args_protect_socket->AddInput("device_path", "o");
    args_protect_socket->AddOutput("succeded", glib2::DataType::DBus<bool>());

    auto args_dco_avail = AddMethod(
        "DcoAvailable",
        [](DBus::Object::Method::Arguments::Ptr args)
        {
#ifdef ENABLE_OVPNDCO
            args->SetMethodReturn(glib2::Value::CreateTupleWrapped(NetCfgDCO::available()));
#else
            args->SetMethodReturn(glib2::Value::CreateTupleWrapped(false));
#endif
        });
    args_dco_avail->AddOutput("available", glib2::DataType::DBus<bool>());


    AddMethod("Cleanup",
              [this](DBus::Object::Method::Arguments::Ptr args)
              {
                  this->method_cleanup_process_resources(args);
                  args->SetMethodReturn(nullptr);
              });

    signals->Debug("Network Configuration service object ready");
    if (!resolver)
    {
        signals->LogWarn("No DNS resolver has been configured");
    }
}


const bool NetCfgServiceHandler::Authorize(const DBus::Authz::Request::Ptr authzreq)
{

    if (DBus::Object::Operation::METHOD_CALL == authzreq->operation)
    {
        const uid_t caller_uid = creds_query->GetUID(authzreq->caller);

        // - NetCfgSubscriptions related:
        //    By default, the subscribe method access is managed by
        //    the D-Bus policy.  The default policy will only allow
        //    this by the openvpn user account.
        if ("net.openvpn.v3.netcfg.NotificationSubscriberList" == authzreq->target)
        {
            // Only allow root to access the subscriber list
            return caller_uid == 0;
        }
        else if ("net.openvpn.v3.netcfg.NotificationUnsubscribe" == authzreq->target)
        {
            if (!subscriptions)
            {
                return false;
            }
            uid_t sub_owner = subscriptions->GetSubscriptionOwner(authzreq->caller);
            signals->Debug("net.openvpn.v3.netcfg.NotificationUnsubscribe: "
                           "owner_uid=" + std::to_string(sub_owner)
                           + ", caller_uid=" + std::to_string(caller_uid));
            return (caller_uid == 0) || (caller_uid == sub_owner);
        }
    }

    // TODO:  Improve with better ACL checks.  Historically, there has not
    //        been much checks, with the exception above.  But this should
    //        be hardened a bit.  Use polkit?
    return true;
}


void NetCfgServiceHandler::method_create_virtual_interface(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    glib2::Utils::checkParams(__func__, params, "(s)");
    std::string device_name = glib2::Value::Extract<std::string>(params, 0);

    signals->Debug(std::string("CreateVirtualInterface(")
                   + "'" + device_name + "')");

    std::string sender = args->GetCallerBusName();
    std::string dev_path = Constants::GenPath("netcfg") + "/"
                           + std::to_string(creds_query->GetPID(sender))
                           + "_" + device_name;

    NetCfgDevice::Ptr device = object_manager->CreateObject<NetCfgDevice>(
        conn,
        object_manager,
        creds_query->GetUID(sender),
        creds_query->GetPID(sender),
        dev_path,
        device_name,
        resolver,
        subscriptions,
        signals->GetLogLevel(),
        signals->GetLogWriter(),
        options);

    signals->LogInfo(std::string("Virtual device '") + device_name + "'"
                     + " registered on " + dev_path
                     + " (owner uid " + std::to_string(creds_query->GetUID(sender))
                     + ", owner pid " + std::to_string(creds_query->GetPID(sender)) + ")");

    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(dev_path, "o"));
}


void NetCfgServiceHandler::method_fetch_interface_list(DBus::Object::Method::Arguments::Ptr args)
{
    std::vector<DBus::Object::Path> dev_paths{};
    bool root_path_found = false;
    for (const auto &[path, dev_obj] : object_manager->GetAllObjects())
    {
        if (!root_path_found && "/net/openvpn/v3/netcfg" == path)
        {
            // We skip "/net/openvpn/v3/netcfg" - that is not a device path
            root_path_found = true;
            continue;
        }

        // If the device object is not null, the path should be valid
        if (dev_obj)
        {
            dev_paths.push_back(path);
        }
    }
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(dev_paths));
}


void NetCfgServiceHandler::method_protect_socket(DBus::Object::Method::Arguments::Ptr args)
{
    GVariant *params = args->GetMethodParameters();
    glib2::Utils::checkParams(__func__, params, "(sbo)", 3);

    pid_t creator_pid = creds_query->GetPID(args->GetCallerBusName());
    std::string remote = glib2::Value::Extract<std::string>(params, 0);
    bool ipv6 = glib2::Value::Extract<bool>(params, 1);
    std::string dev_path = glib2::Value::Extract<std::string>(params, 2);


    // Get the first FD from the fdlist list
    int fd = args->ReceiveFD();

    // If the devpath is valid we get the device name from it to ignore it in the host route to avoid
    // routing loops


    std::string tunif{};
    auto dev = object_manager->GetObject<NetCfgDevice>(dev_path);
    if (dev)
    {
        tunif = dev->get_device_name();
    }

    signals->LogInfo(std::string("Socket protect called for socket ")
                     + std::to_string(fd)
                     + ", remote: '" + remote
                     + "', tun: '" + tunif
                     + "', ipv6: " + (ipv6 ? "yes" : "no")
                     + ", device_path=" + dev_path
                     + ", device-object: " + (dev ? "valid" : "missing"));

    CoreLog::Connect(signals);
    if (options.so_mark >= 0)
    {
        if (fd == -1)
        {
            throw NetCfgException("SO_MARK requested but protect_socket call received no fd");
        }
        openvpn::protect_socket_somark(fd, remote, options.so_mark);
    }
    if (RedirectMethod::BINDTODEV == options.redirect_method)
    {
        if (fd == -1)
        {
            throw NetCfgException("bind to dev method requested but protect_socket call received no fd");
        }
        openvpn::protect_socket_binddev(fd, remote, ipv6);
    }
    if (options.redirect_method == RedirectMethod::HOST_ROUTE)
    {
        openvpn::cleanup_protected_sockets(creator_pid, signals);
        openvpn::protect_socket_hostroute(tunif, remote, ipv6, creator_pid);
    }
    if (fd >= 0)
    {
        close(fd);
    }
    args->SetMethodReturn(glib2::Value::CreateTupleWrapped(true));
}


void NetCfgServiceHandler::method_cleanup_process_resources(DBus::Object::Method::Arguments::Ptr args)
{
    try
    {
        pid_t pid = creds_query->GetPID(args->GetCallerBusName());
        signals->LogInfo(std::string("Cleaning up resources for PID ")
                         + std::to_string(pid) + ".");

        // Just normal loop here, since we delete from the container while modifying it
        for (const auto &it : object_manager->GetAllObjects())
        {
            NetCfgDevice::Ptr tundev = std::static_pointer_cast<NetCfgDevice>(it.second);
            if (tundev->getCreatorPID() == pid)
            {
                // The teardown method will also call to the erase method which will
                // then be a noop but doing the erase here gets us a valid next iterator
                // tundev->teardown();
                object_manager->RemoveObject(tundev->GetPath());
            }
        }
        openvpn::cleanup_protected_sockets(pid, signals);
    }
    catch (const DBus::Signals::Exception &excp)
    {
        std::cerr << __FUNCTION__ << ":" << __LINE__
                  << " -- DBus::Signals::Exception: " << excp.what() << std::endl
                  << "          D-Bus call details:" << args << std::endl;
    }
}
