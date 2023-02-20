#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
#
#

import sys
import dbus
import openvpn3


if len(sys.argv) != 2:
    print("Usage: %s <config-name>" % sys.argv[0])
    sys.exit(1)

# Connect to the Configuration Manager
cfgmgr = openvpn3.ConfigurationManager(dbus.SystemBus())

# Lookup the configuration name
cfg_paths = cfgmgr.LookupConfigName(sys.argv[1])

# Print results
print("Configuration paths found for '%s'" % sys.argv[1])
if len(cfg_paths) > 0:
    for p in cfg_paths:
        print(" - %s" % p)
else:
    print("(No configurations found)")

sys.exit(0)
