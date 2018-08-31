#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018         OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
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
# @file  get-last-status.py
#
# @brief  Simple test program which uses the openvpn3 python module
#         to retrieve the status information for a specific VPN session

import sys
import dbus
import openvpn3
from pprint import pprint

if len(sys.argv) != 2:
    print("Usage: %s <session-path>" % sys.argv[0])
    sys.exit(1)

sm = openvpn3.SessionManager(dbus.SystemBus())
s = sm.Retrieve(sys.argv[1])
pprint(s.GetStatus())
