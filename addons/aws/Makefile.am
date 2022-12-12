#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2020 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2020 - 2022  Lev Stipakov <lev@openvpn.net>
#  Copyright (C) 2020 - 2022  David Sommerseth <davids@openvpn.net>
#

#
# The main program is declared in ../../Makefile.am
# This file only handles data files related to openvpn3-service-aws
#

policy/%.conf : policy/%.conf.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $$(dirname $@) ; \
		sed -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		$< > $@.tmp && mv $@.tmp $@

systemd/%.service : systemd/%.service.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $$(dirname $@) ; \
		sed -e 's|\@LIBEXEC_PATH\@|$(pkglibexecdir)|' \
		$< > $@.tmp && mv $@.tmp $@

aws_certsdir = $(openvpn3_configdir)/awscerts

aws_certificates = \
	af_south_1.crt \
	ap_east_1.crt \
	default.crt \
	eu_south_1.crt \
	me_south_1.crt \
	us_gov.crt

aws_certs_DATA = $(foreach var,$(aws_certificates),certs/$(var))

dbus_system_policy_DATA = \
	policy/net.openvpn.v3.aws.conf

systemd_unit_DATA = \
	systemd/openvpn3-aws.service

EXTRA_DIST = \
	$(aws_certs_DATA) \
	$(foreach var,$(dbus_system_policy_DATA),$(var).in) \
	$(foreach var,$(systemd_unit_DATA),$(var).in)

MAINTAINERCLEANFILES = \
	Makefile.in

CLEANFILES = $(dbus_system_policy_DATA) $(systemd_unit_DATA) *~
