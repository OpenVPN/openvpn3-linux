#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2019      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2019      David Sommerseth <davids@openvpn.net>
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

import sys
import datetime
import dbus
import openvpn3
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

# Global main loop object
mainloop = GLib.MainLoop()


# Simple callback function which is called on each StatusChange
def StatusChange_handler(major, minor, msg):
    maj = openvpn3.StatusMajor(major)
    min = openvpn3.StatusMinor(minor)
    print('%s (%s, %s) %s' % (datetime.datetime.now(), maj, min, msg))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: %s <session path>" % sys.argv[0])
        sys.exit(1)

    # Get a connection to the D-Bus' system bus
    dbusloop = DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus(mainloop=dbusloop)

    # Connect to the session manager
    sessmgr = openvpn3.SessionManager(bus)

    # Get access to the session object
    session = sessmgr.Retrieve(sys.argv[1])

    # Configure the callback function handling StatusChange signals
    session.StatusChangeCallback(StatusChange_handler)

    # Start the main process
    try:
        mainloop.run()
    except KeyboardInterrupt: # Wait for CTRL-C / SIGINT
        print("Exiting")

    sys.exit(0)