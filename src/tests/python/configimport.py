#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
#
#
#  Simple Python example of importing a configuration profile for the
#  configuration manager
#
#  To get an idea of what happens here, look at the introspection
#  result for the net.openvpn.v3.configuration service:
#
#   $ gdbus introspect -y -d net.openvpn.v3.configuration \
#                      -o /net/openvpn/v3/configuration
#
#  And particularly look closely at "interface net.openvpn.v3.configuration"
#  in the output.

import dbus

# Get a connection to the system bus
bus = dbus.SystemBus()

# Retrieve the main configuration manager object
manager_object = bus.get_object('net.openvpn.v3.configuration',
                                '/net/openvpn/v3/configuration')

# Retireve access to the proper interface in the object
config_interface = dbus.Interface(manager_object,
                                  dbus_interface='net.openvpn.v3.configuration')

#
# Here is our super simple configuration file (not valid by the way, lacks --ca)
# all external files must be embedded before it can be imported, otherwise the
# backend client will not be able to read them.
#
config_to_import = """
remote vpn1.example.org
port 30001
proto udp
client-cert-not-required
auth-user-pass
"""

# Import the configuration, and we're given back an object path
config_path = config_interface.Import("Test config",     # name
                                      config_to_import,  # config_str
                                      False,             # single_use
                                      False)             # persistent
print("Configuration path: " + config_path)

