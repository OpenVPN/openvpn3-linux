#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
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
