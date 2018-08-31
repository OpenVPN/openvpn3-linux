#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018      David Sommerseth <davids@openvpn.net>
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
# @file  start-tunnel2.py
#
# @brief  A variation of the start-tunnel.py test program, but this
#         uses the openvpn3 Python module instead of doing all the
#         D-Bus setup locally.
#
#         This REQUIRES Python 3.4 or newer
#

import sys
import time
from getpass import getpass
import dbus
import openvpn3
from openvpn3.constants import StatusMajor, StatusMinor, ClientAttentionType

if len(sys.argv) != 2:
    printf("** Usage: %s <D-Bus configuration path>" % sys.argv[0])
    sys.exit(1)

# Get a connection to the system bus
bus = dbus.SystemBus()

# Get a connection to the Configuration Manager
# and retrieve the configuration profile
cm = openvpn3.ConfigurationManager(bus)
cfg = cm.Retrieve(sys.argv[1])

# Connect to the session manager object
sm = openvpn3.SessionManager(bus)

# Prepare the tunnel
session = sm.NewTunnel(cfg)
print("Session path: " + session.GetPath())
time.sleep(1) # Wait for things to settle down

#
# Start the tunnel
#

# Since the tunnel might need some user input (username/password, etc)
# we need to check if the backend is ready to connect
#
done = False
while not done:
    try:
        session.Ready()       # This throws an exception if not ready

        session.Connect()     # Start the connection
        time.sleep(3)         # Just a simple wait for things to settle. Should poll
                              # for status signals instead.

        session.Ready()       # Check if everything is okay.  If
                              # authentication failed, or a dynamic
                              # challenge is sent, then an exception
                              # is thrown again

        # Simple poll to see if we're connected
        # The 'status' property in a session object contains the last status
        # update sent by the VPN backend
        status = session.GetStatus()

        if status['major'] == StatusMajor.CONNECTION:
            if status['minor'] == StatusMinor.CONN_CONNECTED:
                print("Connected")
                # Simple blocker.  In real-life, more exiting stuff happens here.
                # Should listen for various signals, can capture and store/present
                # Log events, etc.
                #
                # Try running the src/tests/dbus/signal-listener and especially
                # check ProcessChange, StatusChange and Log signals
                #
                try:
                    input("Press enter to disconnect")
                except:
                    pass
            elif status['minor'] == StatusMinor.CONN_DISCONNECTED:
                print("Connection got disconnected:" + status[2])

        session.Disconnect()
        done = True

        print("Disconnected")

    except dbus.exceptions.DBusException as excep:
        if str(excep).find(' Missing user credentials') > 0:
            print("Credentials needed")

            # User credentials comes in tuples of (type, group).
            # Type should always be 1 - that is the type for user credentials.
            for slot in session.FetchUserInputSlots():

                # Skip non-user credentials requests
                if slot.GetTypeGroup()[0] != ClientAttentionType.CREDENTIALS:
                    continue

                # Print the request to the console and send the response back
                # to the VPN backend
                response = None
                try:
                    if False == slot.GetInputMask():
                        response = input(slot.GetLabel() + ': ')
                    else:
                        response = getpass(slot.GetLabel() + ': ')
                except KeyboardInterrupt:
                    # Shutdown if user wants to exit
                    print("\nAborting\n")
                    session.Disconnect()
                    sys.exit(8)

                slot.ProvideInput(response)
        else:
            print(excep)
            done = True
