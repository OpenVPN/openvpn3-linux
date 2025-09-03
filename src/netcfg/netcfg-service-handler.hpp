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
 * @file   netcfg-service-handler.hpp
 *
 * @brief  Declaration the NetCfgServiceHandler.  This is the root service
 *         object of the net.openvpn.v3.netcfg service.
 */

#pragma once

#include <map>
#include <gdbuspp/connection.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/credentials/query.hpp>
#include <gdbuspp/object/base.hpp>
#include <gdbuspp/service.hpp>

#include "log/logwriter.hpp"
#include "dns/settings-manager.hpp"
#include "netcfg-signals.hpp"
#include "netcfg-subscriptions.hpp"
#include "netcfg-options.hpp"


using namespace NetCfg;

class NetCfgDevice;


/**
 *  Root D-Bus object for the net.openvpn.v3.netcfg service, the main
 *  starting point for accessing this service
 *
 */
class NetCfgServiceHandler : public DBus::Object::Base
{
  public:
    using Ptr = std::shared_ptr<NetCfgServiceHandler>;

    /**
     *  Initialize the main Network Configuration service object.  This
     *  is the entrypoint object used by the backend VPN client process
     *
     * @param conn_              DBus::Connection to use
     * @param resolver           DNS::SettingsManager handling DNS resolver
     *                           configuration
     * @param obj_mgr            DBus::Object::Manager which will keep track of
     *                           all the child objects this service creates
     * @param logwr              LogWriter object which handles all the logging
     * @param options            NetCfgOptions containing the base configuration
     *                           of the service
     */
    NetCfgServiceHandler(DBus::Connection::Ptr conn_,
                         DNS::SettingsManager::Ptr resolver,
                         DBus::Object::Manager::Ptr obj_mgr,
                         LogWriter *logwr,
                         NetCfgOptions options);
    ~NetCfgServiceHandler() = default;

    /**
     *  This is required by DBus::Object::Base.  All method calls and property
     *  get/set operations will first be sent via this method to authorize the
     *  access to the requested method or property.
     *
     * @param authzreq  Authz::Request object containing information about
                        the operation and member the caller want to access
     * @return true when access is granted, false rejects the request
     */
    const bool Authorize(const DBus::Authz::Request::Ptr authzreq) override;

  private:
    DBus::Connection::Ptr conn = nullptr;
    DBus::Object::Manager::Ptr object_manager = nullptr;
    DBus::Credentials::Query::Ptr creds_query = nullptr;
    NetCfgSignals::Ptr signals = nullptr;
    DNS::SettingsManager::Ptr resolver = nullptr;
    std::string version{package_version};
    NetCfgOptions options;
    NetCfgSubscriptions::Ptr subscriptions = nullptr;

    /**
     *  D-Bus method - CreateVirtualInterface(s device_name)
     *                 returns: (o) netcfg device D-Bus path
     *
     *  Creates a new virtual network device (tun/dco) with a given name
     *  (provided in the arguments).  This device will be owned by the
     *  D-Bus caller
     *
     * @param args    Object::Method::Arguments with method call details
     */
    void method_create_virtual_interface(DBus::Object::Method::Arguments::Ptr args);


    /**
     *  D-Bus method - FetchInterfaceList()
     *                 returns: (ao) - array of D-Bus paths to devices
     *
     *  Provides a list of all devices the caller has access to.  The list
     *  of devices are D-Bus path to the netcfg object representing the device
     *
     * @param args    Object::Method::Arguments with method call details
     */
    void method_fetch_interface_list(DBus::Object::Method::Arguments::Ptr args);


    /**
     * Reads a unix fd from a connec and protects that socket from being
     * routed over the VPN
     *
     * @param conn   GDBusConnection pointer where the request came
     * @param invoc  GDBusMethodInvocation pointer containing the request.
     *               The file descriptor to be protected must come in the
     *               message inside this request.
     *
     * @return  Returns a GVariant object with the reply to the caller.
     *          This will always be a boolean true value on success.  In case
     *          of errors, a NetCfgException is thrown.
     */
    void method_protect_socket(DBus::Object::Method::Arguments::Ptr args);

    /**
     * Clean up function that removes all objects that are still around from
     * a process like registered virtual devices and socket protections
     */
    void method_cleanup_process_resources(DBus::Object::Method::Arguments::Ptr args);
};
