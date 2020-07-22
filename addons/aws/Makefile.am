#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2020         OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2020         Lev Stipakov <lev@openvpn.net>
#  Copyright (C) 2020         David Sommerseth <davids@openvpn.net>
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
