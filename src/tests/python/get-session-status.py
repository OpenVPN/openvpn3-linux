#!/usr/bin/python3
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2019 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2019 - 2022  David Sommerseth <davids@openvpn.net>
#

import sys
import dbus
import openvpn3

if len(sys.argv) != 2:
    print("Usage: %s <config name>" % sys.argv[0])
    sys.exit(1)

# Connect to the OpenVPN 3 Session Manager
sessionmgr = openvpn3.SessionManager(dbus.SystemBus())

# Get the D-Bus session path from a configuration name
session_paths = sessionmgr.LookupConfigName(sys.argv[1])
if len(session_paths) == 0:
    print("No sessions found")
    sys.exit(0)
elif len(session_paths) > 1:
    print("More than one session path was found, only using the first")

session_path = session_paths[0]
print("Session path: %s" % session_path)

# Get the session object
session = sessionmgr.Retrieve(session_path)

# Query the session object for a status:
status = session.GetStatus()
print("Status:")
for (k,v) in status.items():
    print("  - %s: %s" % (k, v))
