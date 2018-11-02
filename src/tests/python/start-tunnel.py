#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
#
#  Simple Python example of starting a new VPN tunnel based on an
#  existing configuration profile available in the configuration manager.
#  This example will also check if user credentials are needs to be provided.
#
#  To get an idea of what happens here, look at the introspection
#  result for the net.openvpn.v3.sessions service:
#
#   $ gdbus introspect -y -d net.openvpn.v3.sessions \
#                      -o /net/openvpn/v3/sessions
#
#  And particularly look closely at "interface net.openvpn.v3.sessions"
#  in the output.
#
#  Once the NewTunnel() method have been called, a session path is returned.
#  At that point, it is also possible to introspect that object path in the
#  the same net.openvpn.v3.sessions service (destination).
#

import sys
import time
import getpass
import dbus

if len(sys.argv) != 2:
    print("** Usage: %s <D-Bus configuration path>" % sys.argv[0])
    sys.exit(1)

# Get a connection to the system bus
bus = dbus.SystemBus()

# Retrieve the main session manager object
manager_object = bus.get_object('net.openvpn.v3.sessions',
                                '/net/openvpn/v3/sessions')

# Retireve access to the proper interface in the object
sessmgr_interface = dbus.Interface(manager_object,
                                   dbus_interface='net.openvpn.v3.sessions')

# Prepare the tunnel (type casting string to D-Bus objet path variable)
session_path = sessmgr_interface.NewTunnel(dbus.ObjectPath(sys.argv[1]))
print("Session path: " + session_path)
time.sleep(1) # Wait for things to settle down

# Get access to the session object
session_object = bus.get_object('net.openvpn.v3.sessions', session_path)

# Get the proper interface access
session_interface = dbus.Interface(session_object,
                                   dbus_interface='net.openvpn.v3.sessions')
session_properties = dbus.Interface(session_object,
                                    dbus_interface='org.freedesktop.DBus.Properties')

#
# Start the tunnel
#

# Since the tunnel might need some user input (username/password, etc)
# we need to check if the backend is ready to connect
#
done = False
while not done:
    try:
        session_interface.Ready()  # This throws an exception if not ready

        session_interface.Connect() # Start the connection
        time.sleep(3) # Just a simple wait for things to settle. Should poll
                      # for status signals instead.

        session_interface.Ready()   # Check if everything is okay.  If
                                    # authentication failed, or a dynamic
                                    # challenge is sent, then an exception
                                    # is thrown again

        # Simple poll to see if we're connected
        # The 'status' property in a session object contains the last status
        # update sent by the VPN backend
        status = session_properties.Get('net.openvpn.v3.sessions','status')

        if status[0] == 2:        # StatusMajor::CONNECTION
            if status[1] == 7:    # StatusMinor::CONNECTED
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
            elif status[1] == 9:  # StatusMinor:DISCONNECTED
                print("Connection got disconnected:" + status[2])

        session_interface.Disconnect()
        done = True

        print("Disconnected")

    except dbus.exceptions.DBusException as excep:
        if str(excep).find(' Missing user credentials') > 0:
            print("Credentials needed")

            # User credentials comes in tuples of (type, group).
            # Type should always be 1 - that is the type for user credentials.
            for (qtype, qgroup) in session_interface.UserInputQueueGetTypeGroup():

                # Skip non-user credentials requests
                if qtype != 1:
                    continue

                # Print the request to the console and send the response back
                # to the VPN backend
                for qid in session_interface.UserInputQueueCheck(qtype, qgroup):
                    req = session_interface.UserInputQueueFetch(qtype, qgroup, qid)

                    # Field 5 in the request defines if input should be masked or not
                    # Field 4 contains the string to be presented to the user
                    if req[5] == False:
                        response = raw_input(req[4] + ": ")
                    else:
                        response = getpass.getpass(req[4] + ": ")

                    session_interface.UserInputProvide(qtype, qgroup, qid, response)
        else:
            print(excep)
            done = True

