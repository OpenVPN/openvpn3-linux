#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2020        OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2020        Lev Stipakov <lev@openvpn.net>
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

net.openvpn.v3.aws.conf : net.openvpn.v3.aws.conf.in Makefile
	$(AM_V_GEN)sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

MAINTAINERCLEANFILES = \
	Makefile.in

EXTRA_DIST = \
	net.openvpn.v3.aws.conf.in

dbus_system_policy_DATA = \
	net.openvpn.v3.aws.conf

CLEANFILES = $(dbus_system_policy_DATA) *~

