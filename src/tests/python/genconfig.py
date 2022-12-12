#!/usr/bin/python3
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2018 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
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
