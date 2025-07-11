//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  Lev Stipakov <lev@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//  Copyright (C)  Antonio Quartulli <antonio@openvpn.net>
//  Copyright (C)  Heiko Hund <heiko@openvpn.net>
//

#pragma once

#ifdef ENABLE_OVPNDCO

#include <gdbuspp/connection.hpp>
#include <gdbuspp/object/base.hpp>

#ifndef USE_ASIO
#define USE_ASIO
#endif
#include <asio.hpp>

// This needs to be included before any of the OpenVPN 3 Core
// header files.  This is needed to enable the D-Bus logging
// infrastructure for the Core library
#include "log/core-dbus-logger.hpp"

#define OPENVPN_EXTERN extern
#include <openvpn/io/io.hpp>
#include <openvpn/tun/tunmtu.hpp>
#include <openvpn/tun/linux/client/genl.hpp>
#include <openvpn/tun/linux/client/tuncli.hpp>


#include "netcfg-signals.hpp"


class NetCfgDCO : public DBus::Object::Base
{
  public:
    typedef std::shared_ptr<NetCfgDCO> Ptr;
    typedef openvpn::GeNL<NetCfgDCO *> GeNLImpl;

    NetCfgDCO(DBus::Connection::Ptr dbuscon,
              const DBus::Object::Path &objpath,
              const std::string &dev_name,
              pid_t backend_pid,
              LogWriter *logwr);

    ~NetCfgDCO();

    const bool Authorize(const DBus::Authz::Request::Ptr request) override;

    /**
     * Checks for availability of data channel offload kernel module
     *
     * @return bool indicating whether the ovpn-dco kernel module is available
     */
    static bool available();


    /**
     * Called by GeNL in worker thread when there is incoming data or event from kernel
     *
     * @param buf
     */
    void tun_read_handler(openvpn::BufferAllocated &buf);


    /**
     * Deletes ovpn-dco net dev, stops ASIO event loop and GeNL and
     * waits for worker thread to exit.
     */
    void teardown();


  private:
    void method_new_peer(GVariant *params, int fd);
    void method_new_key(GVariant *params);
    void method_swap_keys(GVariant *params);
    void method_set_peer(GVariant *params);

    struct PacketFrom
    {
        typedef std::unique_ptr<PacketFrom> SPtr;
        openvpn::BufferAllocated buf;
    };

    void queue_read_pipe(PacketFrom *);

    std::string backend_bus_name;
    NetCfgSignals::Ptr signals = nullptr;
    int fds[2]; // fds[0] is passed to client, here we use fds[1]
    std::unique_ptr<openvpn_io::posix::stream_descriptor> pipe;
    GeNLImpl::Ptr genl;
    openvpn_io::io_context io_context;
    // thread where ASIO event loop runs, used by GeNL and pipe
    std::unique_ptr<std::thread> th;
    std::unique_ptr<openvpn::AsioWork> asio_work;
    std::string dev_name;
};

#endif // ENABLE_OVPNDCO
