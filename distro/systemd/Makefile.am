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
	$(AM_V_GEN)sed -e 's|\@SBINDIR\@|$(sbindir)|' \
		       -e 's|\@LIBEXECDIR\@|$(pkglibexecdir)|' \
		       -e 's|\@OPENVPN3_CONFIGDIR\@|$(openvpn3_configdir)|' \
		$< > $@.tmp && mv $@.tmp $@

MAINTAINERCLEANFILES = \
	Makefile.in

systemd_unit_DATA = \
	openvpn3-autoload.service \
	openvpn3-session@.service

pkglibexec_SCRIPTS = \
	openvpn3-systemd

EXTRA_DIST = \
	$(foreach var, $(systemd_unit_DATA), $(var).in) \
	$(pkglibexec_SCRIPTS)

CLEANFILES = $(systemd_unit_DATA) *~

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(openvpn3_configdir)"
	$(MKDIR_P) "$(DESTDIR)$(openvpn3_configdir)/autoload"
	chmod 750 "$(DESTDIR)$(openvpn3_configdir)/autoload"
