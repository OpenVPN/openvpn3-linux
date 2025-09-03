#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
#

##
# @file  ConfigManager.py
#
# @brief  Provides a Python class to communicate with the OpenVPN 3
#         configuration manager service over D-Bus
#

import dbus
import json
import time
from functools import wraps


##
#  The Configuration object represents a single configuration profile as
#  presented via the OpenVPN 3 configuration manager D-Bus service.
class Configuration(object):
    ##
    #  Initialize the Configuration object.  It requires a D-Bus connection
    #  object and the D-Bus object path to the configuration profile
    #
    #  @param dbuscon  D-Bus connection object
    #  @param objpath  D-Bus object path to the configuration profile
    #
    def __init__(self, dbuscon, objpath):
        self.__deleted = False
        self.__dbuscon = dbuscon

        # Retrieve access to the configuration object
        self.__config = self.__dbuscon.get_object('net.openvpn.v3.configuration',
                                                  objpath)

        # Retrieve access to the configuration interface in the object
        self.__config_intf = dbus.Interface(self.__config,
                                            dbus_interface="net.openvpn.v3.configuration")

        # Retrieve access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__config,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        self.__cfg_path = objpath


    ##
    #  Internal decorator, checks whether the object has been deleted or not.
    #  If the object has been deleted, throw an exception instead.
    #
    #  For details, lookup how Python decorators work
    #
    def __delete_check(func):
        @wraps(func)
        def __delete_checker(self, *args, **kwargs):
            if self.__deleted is True:
                raise RuntimeError("This configuration object is unavailable")
            return func(self, *args, **kwargs)
        return __delete_checker


    ##
    #  Returns the D-Bus configuration object path
    #
    #  @return String containing the D-Bus object path
    #
    @__delete_check
    def GetPath(self):
        return dbus.ObjectPath(self.__cfg_path)


    ##
    #  Retrieve the configuration name of this object
    #
    #  @return String containing the configuration name of the profile
    #
    @__delete_check
    def GetConfigName(self):
        return str(self.__prop_intf.Get('net.openvpn.v3.configuration','name'))


    ##
    #  Sets a specific property in the configuration profile
    #
    #  @param propname   String containing the property name to modify
    #  @param propvalue  The new value the property should have. The data
    #                    type ov the value must match the data type of the
    #                    property in the D-Bus object
    #
    @__delete_check
    def SetProperty(self, propname, propvalue):
        self.__prop_intf.Set('net.openvpn.v3.configuration',
                             propname, propvalue)


    ##
    #  Retrieve the value of a property in a configuration profile
    #
    #   @param propname  String containing the property name to query
    #
    @__delete_check
    def GetProperty(self, propname):
        return self.__prop_intf.Get('net.openvpn.v3.configuration', propname)


    ##
    #  Adds a tag value to the configuration profile
    #
    #   @param tagvalue  String containing the tag value to add
    #
    @__delete_check
    def AddTag(self, tagvalue):
        if tagvalue.startswith('system:') is True:
            raise RuntimeError('System tags cannot be added by users')

        self.__config_intf.AddTag(tagvalue)


    ##
    #  Remove a tag value to the configuration profile
    #
    #   @param tagvalue  String containing the tag value to remove
    #
    @__delete_check
    def RemoveTag(self, tagvalue):
        if tagvalue.startswith('system:') is True:
            raise RuntimeError('System tags cannot be removed by users')
        self.__config_intf.RemoveTag(tagvalue)


    ##
    #  Retrieve all assigned tags to the configuration profile
    #
    #  @returns Returns a tuple of all user accessible tags
    #
    @__delete_check
    def GetTags(self):
        tags = []
        for t in self.__prop_intf.Get('net.openvpn.v3.configuration','tags'):
            if not t.startswith("system:"):
                tags.append(str(t))
        return tuple(tags)


    ##
    #  Remove the configuration from the configuration manager service
    #
    @__delete_check
    def Remove(self):
        self.__config_intf.Remove()
        self.__deleted = True


    ##
    #  Modifies an override parameter in configuration profile
    #
    #  @param override  String containing the property name to modify
    #  @param value     The new value the property should have. The data
    #                   type ov the value must match the data type of the
    #                   property in the D-Bus object
    #
    @__delete_check
    def SetOverride(self, override, value):
        self.__config_intf.SetOverride(override, value)


    ##
    #  Retrieve all the set overrides
    #
    #   @returns  Returns a dictionary of all overrides with their values
    #
    @__delete_check
    def GetOverrides(self,):
        return self.__prop_intf.Get('net.openvpn.v3.configuration', 'overrides')


    ##
    #  Unset an override setting
    #
    #  @param override  Override to remove
    #
    @__delete_check
    def UnsetOverride(self, override):
        self.__config_intf.UnsetOverride(override)


    ##
    #  Seal the configuration, which makes it impossible to modify it later
    #  on.  This CANNOT be undone.
    #
    @__delete_check
    def Seal(self):
        self.__config_intf.Seal()


    ##
    #  Grant access to this configuration object for a specific user ID
    #
    #  @param  uid   Numeric user ID (uid) to be given access to this object
    #
    @__delete_check
    def AccessGrant(self, uid):
        self.__config_intf.AccessGrant(uid)


    ##
    #  Revoke access from this configuration object for a specific user ID
    #
    #  @param  uid   Numeric user ID (uid) to be given access to this object
    #
    @__delete_check
    def AccessRevoke(self, uid):
        self.__config_intf.AccessRevoke(uid)


    ##
    #  Sets the ownership transfer flag which transfers the owner ship
    #  of the session to the same as the owner of the configuration profile
    #  if the session was started as root
    #
    @__delete_check
    def SetOwnershipTransfer(self, val):
        self.SetProperty('transfer_owner_session', val)


    ##
    #  Validates the configuration profile
    #
    @__delete_check
    def Validate(self):
        return self.__config_intf.Validate()


    ##
    #  Fetch the configuration profile contents as a text/plain string
    #
    @__delete_check
    def Fetch(self):
        return self.__config_intf.Fetch()


    ##
    #  Fetch the configuration profile contents as JSON
    #
    @__delete_check
    def FetchJSON(self):
        return json.loads(self.__config_intf.FetchJSON())


##
#  The ConfigurationManager object provides access to the main object in
#  the configuration manager D-Bus service.  This is primarily used to import
#  new configuration object, but can also be used to retrieve specific objects
#  when the configuration D-Bus object path is known.
#
class ConfigurationManager(object):
    ##
    #  Initialize the ConfigurationManager object
    #
    #  @param  dbuscon   D-Bus connection object to use
    #
    def __init__(self, dbuscon):
        self.__dbuscon = dbuscon

        # Retrieve the main configuration manager object
        self.__manager_object = dbuscon.get_object('net.openvpn.v3.configuration',
                                                   '/net/openvpn/v3/configuration')

        # Retrieve access to the configuration interface in the object
        self.__manager_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface='net.openvpn.v3.configuration')

        # Retrieve access to the property interface in the object
        self.__prop_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface="org.freedesktop.DBus.Properties")

        # Setup a simple access to the Peer interface, for Ping()
        self.__peer_intf = dbus.Interface(self.__manager_object,
                                          dbus_interface='org.freedesktop.DBus.Peer')

    ##
    #  Import a new configuration profile into the configuration manager D-Bus
    #  service.
    #
    #  @param cfgname     String containing a human readable profile name
    #  @param cfg         String containing the complete configuration profile
    #                     itself.  Remember that all external files MUST be
    #                     inlined.
    #  @param single_use  Boolean, is this configuration profile intended to
    #                     only be used once?  If True, this configuration will
    #                     be removed automatically upon the first connection
    #                     attempt.
    #  @param persistent  Boolean, should the configuration be saved to disk on
    #                     the system as a persistent configuration profile?
    #  @param system_tag  String containing a system tag to be assigned during
    #                     import.  Not used by default.
    #
    #  @return Returns a Configuration object of the imported configuration
    #
    def Import(self, cfgname, cfg, single_use, persistent, system_tag = None):
        self.__ping()
        path = self.__manager_intf.Import(cfgname,    #  config name
                                          cfg,        # config profile str
                                          single_use, # Single-use config
                                          persistent) # Persistent config?
        if system_tag is not None:
            # We do this the hard way, as the Configuration object does
            # not allow system tags to be modified (by design)
            cfg_obj = self.__dbuscon.get_object('net.openvpn.v3.configuration',
                                                path)
            cfg_intf = dbus.Interface(cfg_obj, 'net.openvpn.v3.configuration')
            cfg_intf.AddTag('system:' + system_tag)

        return Configuration(self.__dbuscon, path)


    ##
    #  Retrieve a single Configuration object for a specific configuration path
    #
    #  @param objpath   D-Bus object path to the configuration profile to
    #                   retrieve.
    #
    #  @return Returns a Configuration object of the requested configuration
    #          profile
    #
    def Retrieve(self, objpath):
        self.__ping()
        return Configuration(self.__dbuscon, objpath)


    ##
    #  Retrieve a list of all available configuration profiles in the
    #  configuration manager
    #
    def FetchAvailableConfigs(self):
        ret = []
        self.__ping()
        for p in self.__manager_intf.FetchAvailableConfigs():
            ret.append(Configuration(self.__dbuscon, p))
        return ret


    ##
    #  Looks up a configuration name to find available D-Bus paths to
    #  configuration objects with the given name.
    #
    #  @return Returns a list of D-Bus path objects matching the search
    #          criteria
    #
    def LookupConfigName(self, cfgname):
        self.__ping()
        return self.__manager_intf.LookupConfigName(cfgname)


    ##
    #  Retrieves all available configuration profiles with a specific
    #  assigned tag value.
    #
    #  @param tagvalue  Tag value to lookup
    #  @param only_path Boolean flag (default: False) which changes the
    #                   returned result to only be a list of D-Bus paths
    #                   instead of Configuration() objects.
    #
    def SearchByTag(self, tagvalue, only_path=False):
        self.__ping()
        if only_path is True:
            return self.__manager_intf.SearchByTag(tagvalue)
        else:
            ret = []
            for p in self.__manager_intf.SearchByTag(tagvalue):
                ret.append(Configuration(self.__dbuscon, p))
            return ret


    ##
    #  Transfer the ownership of a given configuration path to a new user (UID)
    #
    #  @param cfgpath  D-Bus object path to the configuration profile
    #  @param new_uid  UID of the new owner of this configuration profile
    #
    def TransferOwnership(self, cfgpath, new_uid):
        self.__manager_intf.TransferOwnership(dbus.ObjectPath(cfgpath),
                                              dbus.UInt32(new_uid))


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
                self.__prop_intf.Get('net.openvpn.v3.configuration', 'version')
                return
            except dbus.exceptions.DBusException as excp:
                err = str(excp)
                if err.find("org.freedesktop.DBus.Error.AccessDenied:") > 0:
                    raise RuntimeError("Access denied to the Configuration Manager (ping)")
                time.sleep(delay)
                delay *= 1.33
            attempts -= 1
        raise RuntimeError("Could not establish contact with the Configuration Manager")
