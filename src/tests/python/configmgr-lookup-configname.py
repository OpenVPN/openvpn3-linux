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
