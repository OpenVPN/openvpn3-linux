#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2019         OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as
#  published by the Free Software Foundation, version 3 of the
#  License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

##
# @file  NetCfgManager.py
#
# @brief  Python interface for the net.openvpn.v3.netcfg manager service
#

import dbus
import time
from functools import wraps

from openvpn3 import NetCfgChangeType

class NetworkChangeSignal(object):
    def __init__(self, type, device, details):
        self.__type = NetCfgChangeType(type)
        self.__device = device
        self.__details = details

    def GetType(self):
        return self.__type

    def GetDevice(self):
        return self.__device

    def GetDetails(self, prop):
        return self.__details[prop]

    def GetAllDetails(self):
        return self.__details

    def __repr__(self):
        return '<NetworkChangeSignal type="%s", device="%s">' \
            % (self.__type.name, self.__device)


class NetCfgDevice(object):
    def __init__(self, dbuscon, objpath):
        self.__deleted = False
        self.__dbuscon = dbuscon

        # Retrieve access to the configuration object
        self.__device = self.__dbuscon.get_object('net.openvpn.v3.netcfg',
                                                  objpath)

        # Retrive access to the configuration interface in the object
        self.__device_intf = dbus.Interface(self.__device,
                                            dbus_interface="net.openvpn.v3.netcfg")

        # Retrive access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__device,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        self.__dev_path = objpath
        self.__deleted = False


    ##
    #  Internal decorator, checkes whether the object has been deleted or not.
    #  If the object has been deleted, throw an exception instead.
    #
    #  For details, lookup how Python decorators work
    #
    def __delete_check(func):
        @wraps(func)
        def __delete_checker(self, *args, **kwargs):
            if self.__deleted is True:
                raise RuntimeError("This session object is unavailable")
            return func(self, *args, **kwargs)
        return __delete_checker


    @__delete_check
    def GetProperty(self, prop):
        return self.__prop_intf.Get('net.openvpn.v3.netcfg', prop)


    @__delete_check
    def Destroy(self):
        self.__deleted = True
        self.__device_intf.Destroy()


##
#  The NetCfgManager object provides access to the main object in
#  the network configuration manager D-Bus service.
#
class NetCfgManager(object):
    ##
    #  Initialize the NetCfgManager object
    #
    #  @param  dbuscon   D-Bus connection object to use
    #
    def __init__(self, dbuscon):
        self.__dbuscon = dbuscon

        # Retrieve the main configuration manager object
        self.__manager_object = dbuscon.get_object('net.openvpn.v3.netcfg',
                                                   '/net/openvpn/v3/netcfg')

        # Retireve access to the configuration interface in the object
        self.__manager_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface='net.openvpn.v3.netcfg')

        # Retrive access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        # Setup a simple access to the Peer interface, for Ping()
        self.__peer_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface='org.freedesktop.DBus.Peer')

        self.__networkchange_cbfnc = None
        self.__subscription = None


    def Introspect(self):
        intf = dbus.Interface(self.__manager_object,
                              dbus_interface='org.freedesktop.DBus.Introspectable')
        return intf.Introspect()


    def GetObjectPath(self):
        return self.__manager_object.object_path


    def GetConfiguredDNSservers(self):
        self.__ping()
        return self.__prop_intf.Get('net.openvpn.v3.netcfg',
                                    'global_dns_servers')


    def GetConfiguredDNSsearch(self):
        self.__ping()
        return self.__prop_intf.Get('net.openvpn.v3.netcfg',
                                    'global_dns_search')


    def FetchAllDevices(self):
        self.__ping()

        try:
            ret = []
            for path in self.__manager_intf.FetchInterfaceList():
                ret.append(NetCfgDevice(self.__dbuscon, path))

            return ret

        except dbus.exceptions.DBusException as excp:
            err = str(excp)
            if err.find("org.freedesktop.DBus.Error.AccessDenied:") >= 0:
                raise RuntimeError("Access denied to the "
                                   "NetCfg Manager (FetchInterfaceList)")


    def Retrieve(self, object_path):
        self.__ping()
        return NetCfgDevice(self.__dbuscon, object_path)


    ##
    #  Subscribes to the Log signals for NetCfg events
    #
    #  The callback function needs to accept 3 arguments:
    #     (integer) LogGroup, (integer) LogCategory, (string) message
    #
    def LogCallback(self, cbfnc):
        self.__manager_intf.connect_to_signal('Log', cbfnc)


    ##
    #  Subscribes to the NetworkChange signals for NetCfg events
    #
    #  The callback function needs to accept 1 argument which will always be
    #  a NetworkChangeSignal object
    #
    def SubscribeNetworkChange(self, cbfnc, filter):
        self.__networkchange_cbfnc = cbfnc
        if None == self.__subscription:
            self.__subscription = self.__dbuscon.add_signal_receiver(self.__networkchange_callback,
                                                                     'NetworkChange',
                                                                     'net.openvpn.v3.netcfg',
                                                                     'net.openvpn.v3.netcfg')
            self.__manager_intf.NotificationSubscribe(dbus.UInt32(filter.value))


    def UnsubscribeNetworkChange(self):
        if None == self.__subscription:
            return

        self.__manager_intf.NotificationUnsubscribe("")
        self.__dbuscon.remove_signal_receiver(self.__subscription)
        self.__subscription = None
        self.__networkchange_cbfnc = None


    def __networkchange_callback(self, type, device, details):
        if self.__networkchange_cbfnc is None:
            return
        self.__networkchange_cbfnc(NetworkChangeSignal(type, device, details))


    ##
    #  Private method, which sends a Ping() call to the main D-Bus
    #  interface for the service.  This is used to wake-up the service
    #  if it isn't running yet.
    #
    def __ping(self):
        delay = 0.5
        attempts = 10

        while attempts > 0:
            try:
                self.__peer_intf.Ping()
                self.__prop_intf.Get('net.openvpn.v3.netcfg', 'version')
                return
            except dbus.exceptions.DBusException as excp:
                err = str(excp)
                if err.find("org.freedesktop.DBus.Error.AccessDenied:") > 0:
                    raise RuntimeError("Access denied to the NetCfg Manager (ping)")
                time.sleep(delay)
                delay *= 1.33
            attempts -= 1
        raise RuntimeError("Could not establish contact with the NetCfg Manager")
