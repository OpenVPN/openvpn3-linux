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

#pragma once
#ifdef ENABLE_OVPNDCO

#include <openvpn/common/rc.hpp>
#include <openvpn/io/io.hpp>
#include <openvpn/tun/linux/client/genl.hpp>
#include <openvpn/buffer/buffer.hpp>

#include <openvpn/asio/asiowork.hpp>

#include "dbus/core.hpp"
#include "netcfg-signals.hpp"

class NetCfgDCO : public DBusObject, public RC<thread_safe_refcount>
{
public:
    typedef RCPtr<NetCfgDCO> Ptr;
    typedef GeNL<NetCfgDCO *> GeNLImpl;

    NetCfgDCO(GDBusConnection *dbuscon,
              const std::string& objpath,
              const std::string& dev_name,
              pid_t backend_pid,
              LogWriter *logwr);

    ~NetCfgDCO();

    GVariant* callback_get_property(GDBusConnection *conn,
                                    const std::string sender,
                                    const std::string obj_path,
                                    const std::string intf_name,
                                    const std::string property_name,
                                    GError **error) override;

    GVariantBuilder* callback_set_property(GDBusConnection *conn,
                                           const std::string sender,
                                           const std::string obj_path,
                                           const std::string intf_name,
                                           const std::string property_name,
                                           GVariant *value,
                                           GError **error) override;

    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc) override;

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
    void tun_read_handler(BufferAllocated& buf);

    /**
     * Deletes ovpn-dco net dev, stops ASIO event loop and GeNL and
     * waits for worker thread to exit.
     */
    void teardown();

    void callback_destructor() override;

private:
    void new_key(GVariant *params);

    void swap_keys(GVariant *params);

    struct PacketFrom {
        typedef std::unique_ptr<PacketFrom> SPtr;
        BufferAllocated buf;
    };

    void queue_read_pipe(PacketFrom*);

    std::string backend_bus_name;

    NetCfgSignals signal;

    int fds[2]; // fds[0] is passed to client, here we use fds[1]
    std::unique_ptr<openvpn_io::posix::stream_descriptor> pipe;

    GeNLImpl::Ptr genl;
    openvpn_io::io_context io_context;

    // thread where ASIO event loop runs, used by GeNL and pipe
    std::unique_ptr<std::thread> th;

    std::unique_ptr<AsioWork> asio_work;

    std::string dev_name;
};

#endif  // ENABLE_OVPNDCO
