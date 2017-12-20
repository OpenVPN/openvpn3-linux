#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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
        *~ *.o

AM_CXXFLAGS = \
	-I $(top_srcdir)/src

AM_LDFLAGS = 

noinst_PROGRAMS = \
	gen-python-constants

gen_python_constants_SOURCES = gen-python-constants.cpp
