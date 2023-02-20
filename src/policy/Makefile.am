#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017 - 2023  David Sommerseth <davids@openvpn.net>
#

%.conf : %.conf.in Makefile
	$(AM_V_GEN)sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

%.rules : %.rules.in Makefile
	$(AM_V_GEN)sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

%.pkla : %.pkla.in Makefile
	$(AM_V_GEN)sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

MAINTAINERCLEANFILES = \
	Makefile.in

DBUS_POLICY_FILES = \
	net.openvpn.v3.conf \
	net.openvpn.v3.client.conf \
	net.openvpn.v3.configuration.conf \
	net.openvpn.v3.log.conf \
	net.openvpn.v3.netcfg.conf \
	net.openvpn.v3.sessions.conf

POLKIT_POLICY_FILES = \
	net.openvpn.v3.rules

POLKIT_PKLA_FILES = \
	openvpn3-linux.pkla

EXTRA_DIST = $(foreach var,$(DBUS_POLICY_FILES),$(var).in) \
	$(foreach var,$(POLKIT_POLICY_FILES), $(var).in) \
	$(foreach var,$(POLKIT_PKLA_FILES), $(var).in)

dbus_system_policy_DATA = $(DBUS_POLICY_FILES)

polkit_rules_DATA = $(POLKIT_POLICY_FILES)

if ENABLE_POLKIT_PKLA
polkit_pkla_DATA = $(POLKIT_PKLA_FILES)
endif

dist_noinst_SCRIPTS = syntax-check.sh

CLEANFILES = $(dbus_system_policy_DATA) $(polkit_rules_DATA) $(polkit_pkla_DATA) *~

