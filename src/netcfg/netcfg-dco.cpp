//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2020  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2020  Lev Stipakov <lev@openvpn.net>
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

#ifdef ENABLE_OVPNDCO
#include <openvpn/log/logsimple.hpp>
#include "netcfg-dco.hpp"
#include "netcfg-device.hpp"
#include "dco-keyconfig.pb.h"

#define OPENVPN_EXTERN extern
#include <openvpn/common/base64.hpp>

#include <openvpn/tun/linux/client/tunmethods.hpp>

#include <sys/types.h>
#include <sys/socket.h>

NetCfgDCO::NetCfgDCO(GDBusConnection *dbuscon, const std::string& objpath,
                     const std::string& dev_name, pid_t backend_pid, LogWriter *logwr)
    : DBusObject(objpath + "/dco"),
      signal(dbuscon, LogGroup::NETCFG, objpath, logwr),
      fds{},
      dev_name(dev_name)
{
    std::stringstream introspect;
    introspect << "<node name='" << objpath << "'>"
               << "    <interface name='" << OpenVPN3DBus_interf_netcfg << "'>"
               << "        <method name='NewPeer'>"
               << "          <arg type='u' direction='in' name='peer_id'/>"
               << "          <arg type='s' direction='in' name='sa'/>"
               << "          <arg type='u' direction='in' name='salen'/>"
               << "          <arg type='s' direction='in' name='vpn4'/>"
               << "          <arg type='s' direction='in' name='vpn6'/>"
               << "        </method>"
               << "        <method name='GetPipeFD'/>"
               << "        <method name='NewKey'>"
               << "          <arg type='u' direction='in' name='key_slot'/>"
               << "          <arg type='s' direction='in' name='key_config'/>"
               << "        </method>"
               << "        <method name='SwapKeys'>"
               << "          <arg type='u' direction='in' name='peer_id'/>"
               << "        </method>"
               << "        <method name='SetPeer'>"
               << "          <arg type='u' direction='in' name='peer_id'/>"
               << "          <arg type='u' direction='in' name='keepalive_interval'/>"
               << "          <arg type='u' direction='in' name='keepalive_timeout'/>"
               << "        </method>"
               << "    </interface>"
               << "</node>";
    ParseIntrospectionXML(introspect);

    backend_bus_name = OpenVPN3DBus_name_backends_be + std::to_string(backend_pid);

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
                                this)
                   );
    }
    catch (const std::exception& ex)
    {
        std::string err{"Error initializing GeNL: " + std::string{ex.what()}};
        signal.LogError(err);
        teardown();
        throw NetCfgException(err);
    }

    th.reset(new std::thread([self=Ptr(this)]()
                             {
                                 self->io_context.run();
                             }
                            )
        );

    openvpn_io::post(io_context, [self=Ptr(this)]()
                                 {
                                         self->queue_read_pipe(nullptr);
                                         self->genl->register_packet();
                                 }
                     );
}

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

void NetCfgDCO::queue_read_pipe(PacketFrom *pkt)
{
    if (!pkt)
    {
        pkt = new PacketFrom();
    }
    pkt->buf.reset(0, 2048, 0);
    pipe->async_read_some(pkt->buf.mutable_buffer(),
        [self=Ptr(this), pkt=PacketFrom::SPtr(pkt)](const openvpn_io::error_code& error, const size_t bytes_recvd) mutable
        {
            if (!error)
            {
                uint32_t peer_id;

                if (bytes_recvd < sizeof(peer_id)) {
                    std::stringstream os;
                    os << "Received message too small on pipe, size=" << bytes_recvd;
                    self->signal.LogError(os.str());
                } else {
                    std::memcpy(&peer_id, pkt->buf.data(), sizeof(peer_id));
                    self->genl->send_data(peer_id, pkt->buf.data() + sizeof(peer_id),
                                          bytes_recvd - sizeof(peer_id));
                }
                self->queue_read_pipe(pkt.release());
            }
        }
    );
}

void NetCfgDCO::callback_destructor()
{
    teardown();
}

NetCfgDCO::~NetCfgDCO()
{
#ifdef OPENVPN_DEBUG
    // this can be called from worker thread
    signal.Debug("NetCfgDCO::~NetCfgDCO");
#endif
}

bool NetCfgDCO::available()
{
    return GeNLImpl::available();
}

void NetCfgDCO::tun_read_handler(BufferAllocated &buf)
{
    pipe->write_some(buf.const_buffer());
}

GVariant* NetCfgDCO::callback_get_property(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GError **error)
{
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                "Unknown property");
    return nullptr;
}

GVariantBuilder* NetCfgDCO::callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
{
    throw NetCfgException("Not implemented");
}

void NetCfgDCO::callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
{
    try
    {
        IdleCheck_UpdateTimestamp();

        GVariant *retval = nullptr;

        if ("GetPipeFD" == method_name)
        {
            prepare_invocation_fd_results(invoc, nullptr, fds[0]);
            return;
        }
        else if ("NewPeer" == method_name)
        {
            GLibUtils::checkParams(__func__, params, "(ususs)", 5);

            int transport_fd = GLibUtils::get_fd_from_invocation(invoc);

            unsigned int peer_id = GLibUtils::ExtractValue<unsigned int>(params, 0);
            std::string sa_str = GLibUtils::ExtractValue<std::string>(params, 1);
            int salen = GLibUtils::ExtractValue<unsigned int>(params, 2);
            std::string vpn4_str = GLibUtils::ExtractValue<std::string>(params, 3);
            std::string vpn6_str = GLibUtils::ExtractValue<std::string>(params, 4);

            struct sockaddr_storage sa;
            int ret = base64->decode(&sa, sizeof(sa), sa_str);
            if (ret != salen)
            {
                throw NetCfgException("Sockaddr size mismatching");
            }
            IPv4::Addr vpn4 = IPv4::Addr::from_string(vpn4_str);
            IPv6::Addr vpn6 = IPv6::Addr::from_string(vpn6_str);

            openvpn_io::post(io_context, [peer_id, transport_fd, sa, salen, vpn4, vpn6, self=Ptr(this)]()
                                         {
                                             self->genl->new_peer(peer_id, transport_fd, (struct sockaddr *)&sa, salen, vpn4, vpn6);
                                         }
                );

            retval = g_variant_new("()");
        } else if ("NewKey" == method_name)
        {
            new_key(params);
        } else if ("SwapKeys" == method_name)
        {
            swap_keys(params);
        }
        else if ("SetPeer" == method_name)
        {
            GLibUtils::checkParams(__func__, params, "(uuu)", 3);

            unsigned int peer_id;
            int keepalive_interval, keepalive_timeout;
            g_variant_get(params, "(uuu)", &peer_id,
                          &keepalive_interval, &keepalive_timeout);

            openvpn_io::post(io_context, [peer_id,
                                          keepalive_interval,
                                          keepalive_timeout,
                                          self=Ptr(this)]()
                             {
                                     self->genl->set_peer(peer_id,
                                                          keepalive_interval,
                                                          keepalive_timeout);
                             });

            retval = g_variant_new("()");
        }

        g_dbus_method_invocation_return_value(invoc, retval);
    }
    catch (DBusCredentialsException& excp)
    {
        signal.LogCritical(excp.what());
        excp.SetDBusError(invoc);
    }
    catch (const std::exception& excp)
    {
        std::string errmsg = "Failed executing D-Bus call '"
                            + method_name + "': " + excp.what();
        GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.netcfg.error.generic",
                                                      errmsg.c_str());
        g_dbus_method_invocation_return_gerror(invoc, err);
        g_error_free(err);
    }
    catch (...)
    {
        GError *err = g_dbus_error_new_for_dbus_error("net.openvpn.v3.netcfg.error.unspecified",
                                                      "Unknown error");
        g_dbus_method_invocation_return_gerror(invoc, err);
        g_error_free(err);
    }
}

void NetCfgDCO::new_key(GVariant* params)
{
    GLibUtils::checkParams(__func__, params, "(us)", 2);

    int key_slot = g_variant_get_uint32(g_variant_get_child_value(params, 0));

    DcoKeyConfig dco_kc;
    dco_kc.ParseFromString(base64->decode(g_variant_get_string(g_variant_get_child_value(params, 1), 0)));

    auto copyKeyDirection = [](const DcoKeyConfig_KeyDirection& src, KoRekey::KeyDirection& dst)
        {
            dst.cipher_key = reinterpret_cast<const unsigned char*>(src.cipher_key().data());
            std::memcpy(dst.nonce_tail, src.nonce_tail().data(), sizeof(dst.nonce_tail));
            dst.cipher_key_size = src.cipher_key_size();
        };

    openvpn_io::post(io_context, [=, self=Ptr(this)]()
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

                                     self->genl->new_key(key_slot, &kc);
                                 }
        );
}

void NetCfgDCO::swap_keys(GVariant *params)
{
    GLibUtils::checkParams(__func__, params, "(u)", 1);

    unsigned int peer_id = GLibUtils::ExtractValue<unsigned int>(params, 0);

    openvpn_io::post(io_context, [=, self=Ptr(this)]()
    {
        self->genl->swap_keys(peer_id);
    });
}
#endif  // ENABLE_OVPNDCO
