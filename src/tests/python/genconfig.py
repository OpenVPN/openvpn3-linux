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
# @file  genconfig.py
#
# @brief  Simple test program which uses the openvpn3 python module
#         to generate a proper configuration with external files embedded.
#         It uses the same configuration parser as the openvpn2 front-end to
#         openvpn3, and should support most the same set of options as the
#         classic OpenVPN 2.x client related options.

import sys
import openvpn3

cfgpars = openvpn3.ConfigParser(sys.argv,
				"Generate an OpenVPN configuration profile from the command line")
print(cfgpars.GenerateConfig())
