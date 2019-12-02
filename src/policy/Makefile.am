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

%.conf : %.conf.in Makefile
	$(AM_V_GEN)sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

MAINTAINERCLEANFILES = \
	Makefile.in

POLICY_FILES = \
	net.openvpn.v3.conf \
	net.openvpn.v3.client.conf \
	net.openvpn.v3.configuration.conf \
	net.openvpn.v3.log.conf \
	net.openvpn.v3.netcfg.conf \
	net.openvpn.v3.sessions.conf

EXTRA_DIST = $(foreach var,$(POLICY_FILES),$(var).in)

dbus_system_policy_DATA = $(POLICY_FILES)

dist_noinst_SCRIPTS = syntax-check.sh

CLEANFILES = $(dbus_system_policy_DATA) *~

