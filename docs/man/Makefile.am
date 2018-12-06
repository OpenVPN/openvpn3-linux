#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018      David Sommerseth <davids@openvpn.net>
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

% : %.rst Makefile
	$(AM_V_GEN)rst2man $< > $@

MAN_SECTION_8 = \
	openvpn2.8 \
	openvpn3.8 \
	openvpn3-autoload.8

EXTRA_DIST = \
	$(foreach var,$(MAN_SECTION_8),$(var).rst)

if HAVE_RST2MAN
man8_MANS = $(MAN_SECTION_8)
endif

MAINTAINERCLEANFILES = \
	Makefile.in

CLEANFILES = $(MAN_SECTION_8) *~
