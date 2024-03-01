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

#include "build-config.h"

#ifdef ENABLE_OVPNDCO
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>

// These must be included *before* the openvpn/
// include files, otherwise the compilation will
// fail.  The OpenVPN 3 Core library is picky on
// the #include order.  And these local include
// files prepares the ground for the Core library.
#include "dbus/constants.hpp"
#include "dco/dco-keyconfig.pb.h"
#include "netcfg-dco.hpp"
#include "netcfg-device.hpp"

#ifndef OPENVPN_EXTERN
#define OPENVPN_EXTERN extern
#endif
#include <openvpn/common/base64.hpp>
#include <openvpn/tun/tunmtu.hpp>
#include <openvpn/tun/linux/client/tunnetlink.hpp>
#include <openvpn/tun/linux/client/tunmethods.hpp>
#include <openvpn/buffer/buffer.hpp>
#include <openvpn/asio/asiowork.hpp>


NetCfgDCO::NetCfgDCO(DBus::Connection::Ptr dbuscon,
              const DBus::Object::Path &objpath,
              const std::string &dev_name,
              pid_t backend_pid,
              LogWriter *logwr)
    : DBus::Object::Base(objpath + "/dco", Constants::GenInterface("netcfg")),
      fds{},
      dev_name(dev_name)
{
    // Need its own signals object, since the D-Bus path is different
    signals = NetCfgSignals::Create(dbuscon, LogGroup::NETCFG, GetPath(), logwr);
    RegisterSignals(signals);

    auto new_peer = AddMethod("NewPeer",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  this->method_new_peer(args->GetMethodParameters(),
                                                        args->ReceiveFD());
                                  args->SetMethodReturn(nullptr);
                              });
    new_peer->PassFileDescriptor(DBus::Object::Method::PassFDmode::RECEIVE);
    new_peer->AddInput("peer_id", glib2::DataType::DBus<uint32_t>());
    new_peer->AddInput("sa", glib2::DataType::DBus<std::string>());
    new_peer->AddInput("salen", glib2::DataType::DBus<uint32_t>());
    new_peer->AddInput("vpn4", glib2::DataType::DBus<std::string>());
    new_peer->AddInput("vpn6", glib2::DataType::DBus<std::string>());

    auto get_pipe_fd = AddMethod("GetPipeFD",
                                 [this](DBus::Object::Method::Arguments::Ptr args)
                                 {
                                     args->SendFD(this->fds[0]);
                                     args->SetMethodReturn(nullptr);
                                 });
    get_pipe_fd->PassFileDescriptor(DBus::Object::Method::PassFDmode::SEND);

    auto new_key = AddMethod("NewKey",
                             [this](DBus::Object::Method::Arguments::Ptr args)
                             {
                                 this->method_new_key(args->GetMethodParameters());
                                 args->SetMethodReturn(nullptr);
                             });
    new_key->AddInput("key_slot", glib2::DataType::DBus<uint32_t>());
    new_key->AddInput("key_config", glib2::DataType::DBus<std::string>());

    auto swap_keys = AddMethod("SwapKeys",
                               [this](DBus::Object::Method::Arguments::Ptr args)
                               {
                                   this->method_swap_keys(args->GetMethodParameters());
                                   args->SetMethodReturn(nullptr);
                               });
    swap_keys->AddInput("peer_id", glib2::DataType::DBus<uint32_t>());

    auto set_peer = AddMethod("SetPeer",
                              [this](DBus::Object::Method::Arguments::Ptr args)
                              {
                                  this->method_set_peer(args->GetMethodParameters());
                                  args->SetMethodReturn(nullptr);
                              });
    set_peer->AddInput("peer_id", glib2::DataType::DBus<uint32_t>());
    set_peer->AddInput("keepalive_interval", glib2::DataType::DBus<uint32_t>());
    set_peer->AddInput("keepalive_timeout", glib2::DataType::DBus<uint32_t>());

    backend_bus_name = Constants::GenServiceName("backends.be")
                       + std::to_string(backend_pid);

    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    std::ostringstream os;
    if (TunNetlink::iface_new(os, dev_name, "ovpn-dco") != 0)
    {
        throw NetCfgException("Error creating ovpn-dco device: " + os.str());
    }

    pipe.reset(new openvpn_io::posix::stream_descriptor(io_context, fds[1]));

    asio_work.reset(new AsioWork(io_context));

    try
    {
        genl.reset(new GeNLImpl(io_context,
                                if_nametoindex(dev_name.c_str()),
                                this));
    }
    catch (const std::exception &ex)
    {
        std::string err{"Error initializing GeNL: " + std::string{ex.what()}};
        signals->LogError(err);
        teardown();
        throw NetCfgException(err);
    }

    th.reset(new std::thread([this]()
                             {
                                 this->io_context.run();
                             }));
}


NetCfgDCO::~NetCfgDCO()
{
    try
    {
#ifdef OPENVPN_DEBUG
        signals->Debug("NetCfgDCO::~NetCfgDCO");
#endif
        teardown();
    }
    catch (const std::exception &excp)
    {
        signals->LogCritical("~NetCfgDCO: " + std::string(excp.what()));
    }
}


const bool NetCfgDCO::Authorize(const DBus::Authz::Request::Ptr request)
{
    return true;
};


void NetCfgDCO::teardown()
{
    ::close(fds[0]); // fds[1] will be closed by pipe dctor

    if (genl)
    {
        genl->stop();
    }

    if (pipe)
    {
        pipe->close();
    }

    if (asio_work)
    {
        asio_work.reset();
    }

    io_context.stop();

    std::ostringstream os;
    TunNetlink::iface_del(os, dev_name);

    if (th && th->joinable())
    {
        th->join();
    }
}


bool NetCfgDCO::available()
{
    return GeNLImpl::available();
}


void NetCfgDCO::tun_read_handler(BufferAllocated &buf)
{
    try
    {
        pipe->write_some(buf.const_buffer());
    }
    catch (const std::system_error &err)
    {
        std::ostringstream msg;
        msg << "NetCfgDCO [" << dev_name << "] ERROR (tun_read_handler): "
            << err.what();
        if (signals)
        {
            signals->LogCritical(msg.str());
        }
        else
        {
            std::cerr << msg.str() << std::endl;
        }
    }
}


void NetCfgDCO::method_new_peer(GVariant *params, int transport_fd)
{
    glib2::Utils::checkParams(__func__, params, "(ususs)", 5);

    unsigned int peer_id = glib2::Value::Extract<unsigned int>(params, 0);
    std::string sa_str = glib2::Value::Extract<std::string>(params, 1);
    int salen = glib2::Value::Extract<unsigned int>(params, 2);
    std::string vpn4_str = glib2::Value::Extract<std::string>(params, 3);
    std::string vpn6_str = glib2::Value::Extract<std::string>(params, 4);

    struct sockaddr_storage sa;
    int ret = base64->decode(&sa, sizeof(sa), sa_str);
    if (ret != salen)
    {
        throw NetCfgException("Sockaddr size mismatching");
    }
    IPv4::Addr vpn4 = IPv4::Addr::from_string(vpn4_str);
    IPv6::Addr vpn6 = IPv6::Addr::from_string(vpn6_str);

    openvpn_io::post(io_context,
                     [peer_id, transport_fd, sa, salen, vpn4, vpn6, this]()
                     {
                         this->genl->new_peer(peer_id,
                                              transport_fd,
                                              (struct sockaddr *)&sa,
                                              salen,
                                              vpn4,
                                              vpn6);
                     });
}

void NetCfgDCO::method_new_key(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(us)", 2);

    int key_slot = glib2::Value::Extract<uint32_t>(params, 0);
    std::string key_config = glib2::Value::Extract<std::string>(params, 1);

    DcoKeyConfig dco_kc;
    dco_kc.ParseFromString(base64->decode(key_config));

    auto copyKeyDirection = [](const DcoKeyConfig_KeyDirection &src, KoRekey::KeyDirection &dst)
    {
        dst.cipher_key = reinterpret_cast<const unsigned char *>(src.cipher_key().data());
        std::memcpy(dst.nonce_tail, src.nonce_tail().data(), sizeof(dst.nonce_tail));
        dst.cipher_key_size = src.cipher_key_size();
    };

    openvpn_io::post(io_context,
                     [=]()
                     {
                         KoRekey::KeyConfig kc;
                         std::memset(&kc, 0, sizeof(kc));
                         kc.key_id = dco_kc.key_id();
                         kc.remote_peer_id = dco_kc.remote_peer_id();
                         kc.cipher_alg = dco_kc.cipher_alg();

                         copyKeyDirection(dco_kc.encrypt(),
                                          kc.encrypt);
                         copyKeyDirection(dco_kc.decrypt(),
                                          kc.decrypt);

                         this->genl->new_key(key_slot, &kc);
                     });
}


void NetCfgDCO::method_swap_keys(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(u)", 1);

    unsigned int peer_id = glib2::Value::Extract<unsigned int>(params, 0);

    openvpn_io::post(io_context,
                     [=]()
                     {
                         this->genl->swap_keys(peer_id);
                     });
}


void NetCfgDCO::method_set_peer(GVariant *params)
{
    glib2::Utils::checkParams(__func__, params, "(uuu)", 3);

    uint32_t peer_id = glib2::Value::Extract<uint32_t>(params, 0);
    uint32_t keepalive_interval = glib2::Value::Extract<uint32_t>(params, 1);
    uint32_t keepalive_timeout = glib2::Value::Extract<uint32_t>(params, 2);

    openvpn_io::post(io_context,
                     [peer_id, keepalive_interval, keepalive_timeout, this]()
                     {
                         this->genl->set_peer(peer_id,
                                              keepalive_interval,
                                              keepalive_timeout);
                     });
}
#endif // ENABLE_OVPNDCO
