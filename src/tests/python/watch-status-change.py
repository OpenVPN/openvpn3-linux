#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
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
