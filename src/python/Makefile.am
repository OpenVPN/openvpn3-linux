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
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/openvpn3-core \
	$(OPENVPN3_CORE_CXXFLAGS) \
	$(LIBGLIBGIO_CFLAGS) $(LIBGLIBGIOUNIX_CFLAGS)

CLEANFILES = \
	*~ \
	openvpn3/*.pyc        \
	openvpn3/constants.py


noinst_PROGRAMS = \
	openvpn3/gen-python-constants

BUILT_SOURCES = openvpn3/constants.py

pkgopenvpn3dir = $(pythondir)/openvpn3
pkgopenvpn3_PYTHON = \
	openvpn3/__init__.py     \
	openvpn3/ConfigParser.py \
	openvpn3/ConfigManager.py \
	openvpn3/NetCfgManager.py \
	openvpn3/SessionManager.py

nodist_pkgopenvpn3_PYTHON = \
	openvpn3/constants.py

dist_bin_SCRIPTS = openvpn2 openvpn3-as
dist_sbin_SCRIPTS = openvpn3-autoload

openvpn3_gen_python_constants_SOURCES = openvpn3/gen-python-constants.cpp

openvpn3/constants.py : openvpn3/gen-python-constants
	$(AM_V_GEN)$(top_builddir)/src/python/openvpn3/gen-python-constants > openvpn3/constants.py
	-touch --reference=$(top_srcdir)/src/python/openvpn3/gen-python-constants.cpp openvpn3/constants.py

pkgopenvpn3 : openvpn3/constants.py

