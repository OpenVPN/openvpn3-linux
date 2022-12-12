#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017-2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017-2022  David Sommerseth <davids@openvpn.net>
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
