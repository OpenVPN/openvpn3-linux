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

#include <openvpn/tun/linux/client/tunmethods.hpp>

#include <sys/types.h>
#include <sys/socket.h>

NetCfgDCO::NetCfgDCO(GDBusConnection *dbuscon, const std::string& objpath,
                     const std::string& dev_name, int transport_fd,
                     pid_t backend_pid, LogWriter *logwr)
    : DBusObject(objpath + "/dco"),
      signal(dbuscon, LogGroup::NETCFG, objpath, logwr),
      fds{},
      dev_name(dev_name)
{
    std::stringstream introspect;
    introspect << "<node name='" << objpath << "'>"
               << "    <interface name='" << OpenVPN3DBus_interf_netcfg << "'>"
               << "        <method name='NewPeer'>"
               << "          <arg type='s' direction='in' name='local_ip'/>"
               << "          <arg type='u' direction='in' name='local_port'/>"
               << "          <arg type='s' direction='in' name='remote_ip'/>"
               << "          <arg type='u' direction='in' name='remote_port'/>"
               << "        </method>"
               << "        <method name='GetPipeFD'/>"
               << "    </interface>"
               << "</node>";
    ParseIntrospectionXML(introspect);

    backend_bus_name = OpenVPN3DBus_name_backends_be + std::to_string(backend_pid);

    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    std::ostringstream os;
    TunNetlink::iface_new(os, dev_name, "ovpn-dco");

    pipe.reset(new openvpn_io::posix::stream_descriptor(io_context, fds[1]));

    asio_work.reset(new AsioWork(io_context));

    th.reset(new std::thread([self=Ptr(this)]()
                             {
                                 self->io_context.run();
                             }
                            )
        );

    genl.reset(new GeNLImpl(io_context,
                            if_nametoindex(dev_name.c_str()),
                            this));

    openvpn_io::post(io_context, [transport_fd, self=Ptr(this)]()
                                 {
                                         self->queue_read_pipe(nullptr);
                                         self->genl->start_vpn(transport_fd);
                                 }
                     );
}

void NetCfgDCO::teardown()
{
    ::close(fds[0]); // fds[1] will be closed by pipe dctor

    genl->stop();
    pipe->close();
    asio_work.reset();
    io_context.stop();

    std::ostringstream os;
    TunNetlink::iface_del(os, dev_name);

    th->join();
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
                self->genl->send_data(pkt->buf.data(), bytes_recvd);
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
    signal.Debug("NetCfgDCO::~NetCfgDCO");
#endif
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
            int local_port, remote_port;
            gchar *local_ip, *remote_ip;
            g_variant_get(params, "(susu)", &local_ip, &local_port, &remote_ip, &remote_port);

            openvpn_io::ip::udp::endpoint local(openvpn_io::ip::make_address(local_ip), local_port);
            openvpn_io::ip::udp::endpoint remote(openvpn_io::ip::make_address(remote_ip), remote_port);

            openvpn_io::post(io_context, [local, remote, self=Ptr(this)]()
                                         {
                                             self->genl->new_peer(local, remote);
                                         }
                );

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
#endif  // ENABLE_OVPNDCO
