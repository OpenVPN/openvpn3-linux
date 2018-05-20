#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017-2018 OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017-2018 David Sommerseth <davids@openvpn.net>
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

AM_CXXFLAGS = \
	-I$(top_srcdir)/src

CLEANFILES = \
	openvpn3/constants.py

.PHONY: openvpn3_constants.py

noinst_PROGRAMS = \
	openvpn3/gen-python-constants

BUILT_SOURCES = openvpn3/constants.py

openvpn3_gen_python_constants_SOURCES = openvpn3/gen-python-constants.cpp

openvpn3/constants.py : openvpn3/gen-python-constants
	 $(AM_V_GEN)openvpn3/gen-python-constants > openvpn3/constants.py
