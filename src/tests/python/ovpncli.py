#!/usr/bin/python3
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017 - 2018  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017 - 2018  David Sommerseth <davids@openvpn.net>
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
# @file  ovpncli.py
#
# @brief  Simple test program for the openvpn3 Python module
#         This test program will basically work similar to the openvpn2
#         front-end, but not as robust.

import sys
import dbus
import time
import openvpn3
from getpass import getpass

# Parse the command line arguments
cmdparser = openvpn3.ConfigParser(sys.argv, 'OpenVPN 3 module test program')
cmdparser.SanityCheck()

# Get a configuration name, if avialable
cfgname = cmdparser.GetConfigName() and cmdparser.GetConfigName() or '(unknown)'

# Get connected to the D-Bus system bus
bus = dbus.SystemBus()

# Get a connection to the openvpn3-service-configmgr service
# and import the configuration
cm = openvpn3.ConfigurationManager(bus)
cfg = cm.Import(cfgname, cmdparser.GenerateConfig(), False, False)
print("Configuration D-Bus path: " + cfg.GetPath())

# Get a connection to the openvpn3-service-sessionmgr service
# and start a new tunnel with the just imported config profile
sm = openvpn3.SessionManager(bus)
session = sm.NewTunnel(cfg)
print("Session D-Bus path: " + session.GetPath())

# Wait for the backends to settle
# The GetStatus() method will throw an exception
# if the backend is not yet ready
ready = False
while not ready:
    try:
        print("+ Status: " + str(session.GetStatus()))
        ready = True
    except dbus.exceptions.DBusException:
        # If no status is available yet, wait and retry
        time.sleep(1)


# Start the VPN connection
ready = False
while not ready:
    try:
        # Is the backend ready to connect?  If not an exception is thrown
        session.Ready()
        session.Connect()
        ready = True

    except dbus.exceptions.DBusException as e:
        # If this is not about user credentials missing, re-throw the exception
        if str(e).find(' Missing user credentials') < 1:
            raise e

        # Query the user for all information the backend has requested
        for u in session.FetchUserInputSlots():
            # We only care about responding to credential requests here
            if u.GetTypeGroup()[0] != openvpn3.ClientAttentionType.CREDENTIALS:
                continue

            # If the user input is to be masked, use getpass() otherwise input()
            query = u.GetInputMask() and getpass or input

            # Query the user for the information and
            # send it back to the backend
            u.ProvideInput(query(u.GetLabel() + ': '))

            # Now the while-loop will ensure session.Ready() is re-run

# Wait 15 seconds for the backend to get a connection
for i in range(1, 16):
    print("[%i] Status: %s" % (i, str(session.GetStatus())))
    time.sleep(1)

# Remove the config profile from the configuration manager service
cfg.Remove()
