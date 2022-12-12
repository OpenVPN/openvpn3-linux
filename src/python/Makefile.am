#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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

