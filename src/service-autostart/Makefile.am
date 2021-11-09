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

%.service : %.service.in Makefile
	$(AM_V_GEN)sed -e 's|\@LIBEXEC_PATH\@|$(pkglibexecdir)|' \
		       -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		       -e 's|\@OPENVPN_STATEDIR\@|$(openvpn3_statedir)|' \
		       -e 's|\@DNS_RESOLVER_CONFIG\@|$(dns_resolver_config)|' \
		$< > $@.tmp && mv $@.tmp $@

MAINTAINERCLEANFILES = \
	Makefile.in

EXTRA_DIST = \
	net.openvpn.v3.configuration.service.in \
	net.openvpn.v3.sessions.service.in \
	net.openvpn.v3.backends.service.in \
	net.openvpn.v3.netcfg.service.in \
	net.openvpn.v3.log.service.in

dbus_system_service_DATA = \
	net.openvpn.v3.configuration.service \
	net.openvpn.v3.sessions.service \
	net.openvpn.v3.backends.service \
	net.openvpn.v3.netcfg.service \
	net.openvpn.v3.log.service

CLEANFILES = $(dbus_system_service_DATA) *~

# If chown fails, we will ignore that - most likely not run as root
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(openvpn3_statedir) $(DESTDIR)$(openvpn3_statedir)/configs
	chmod 750 $(DESTDIR)$(openvpn3_statedir) $(DESTDIR)$(openvpn3_statedir)/configs
	chown -R $(OPENVPN_USERNAME):$(OPENVPN_GROUP) $(DESTDIR)$(openvpn3_statedir) || echo
