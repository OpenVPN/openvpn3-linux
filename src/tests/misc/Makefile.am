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

MAINTAINERCLEANFILES = \
	Makefile.in

CLEANFILES = \
        *~

AM_CXXFLAGS = \
	$(OPENVPN3_CORE_CXXFLAGS) \
	$(LIBGLIBGIO_CFLAGS) \
	$(LIBJSONCPP_CFLAGS) -DOPENVPN_NO_IO \
	-I $(top_srcdir)/src

AM_LDFLAGS = \
	$(LIBGLIBGIO_LIBS) \
	$(LIBJSONCPP_LIBS)


noinst_PROGRAMS = \
	config-export-json-test \
	json-config-import-test \
	lookup-tests

config_export_json_test_SOURCES = config-export-json-test.cpp

json_config_import_test_SOURCES = json-config-import-test.cpp

lookup_tests_SOURCES = lookup-tests.cpp