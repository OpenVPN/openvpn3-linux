#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018 - 2021  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018 - 2021  David Sommerseth <davids@openvpn.net>
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

% : %.in
	$(AM_V_GEN)sed -e 's|\@LIBEXEC_PATH\@|$(pkglibexecdir)|' \
		       -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		       -e 's|\@OPENVPN_STATEDIR\@|$(openvpn3_statedir)|' \
		$< > $@.tmp && mv $@.tmp $@


# This keeps track of all man pages for addons we must always package
ADDONS_MAN_PACKAGE =

# This keeps track of all man pages for addons which are conditionaly built
ADDONS_SECTION_8 =

#
# Add-ons man page: openvpn3-service-aws
#
ADDONS_AWS_SECTION_8 = openvpn3-service-aws.8
ADDONS_MAN_PACKAGE += $(ADDONS_AWS_SECTION_8)
if BUILD_ADDONS_AWS
ADDONS_SECTION_8 += $(ADDONS_AWS_SECTION_8)
endif

#
# Default OpenVPN 3 Linux man pages
#
MAN_SECTION_1 = \
	openvpn2.1 \
	openvpn3.1 \
	openvpn3-as.1 \
	openvpn3-config-acl.1 \
	openvpn3-config-manage.1 \
	openvpn3-config-import.1 \
	openvpn3-config-remove.1 \
	openvpn3-config-dump.1 \
	openvpn3-configs-list.1 \
	openvpn3-log.1 \
	openvpn3-session-acl.1 \
	openvpn3-session-auth.1 \
	openvpn3-session-manage.1 \
	openvpn3-session-start.1 \
	openvpn3-session-stats.1 \
	openvpn3-sessions-list.1


MAN_SECTION_7 = \
	openvpn3-linux.7

MAN_SECTION_8 = \
	openvpn3-admin.8 \
	openvpn3-admin-log-service.8 \
	openvpn3-admin-netcfg-service.8 \
	openvpn3-admin-sessionmgr-service.8 \
	openvpn3-autoload.8 \
	openvpn3-service-backendstart.8 \
	openvpn3-service-client.8 \
	openvpn3-service-configmgr.8 \
	openvpn3-service-logger.8 \
	openvpn3-service-netcfg.8 \
	openvpn3-service-sessionmgr.8 \
	openvpn3-systemd.8 \
	$(ADDONS_SECTION_8)

EXTRA_DIST = \
	$(foreach var,$(MAN_SECTION_1),$(var).rst) \
	$(foreach var,$(MAN_SECTION_7),$(var).rst) \
	$(foreach var,$(MAN_SECTION_8),$(var).rst) \
	$(foreach var,$(ADDONS_MAN_PACKAGE),$(var).rst)


if HAVE_RST2MAN
man1_MANS = $(MAN_SECTION_1)

man7_MANS = $(MAN_SECTION_7)

man8_MANS = $(MAN_SECTION_8)
endif

MAINTAINERCLEANFILES = \
	Makefile.in

CLEANFILES = $(MAN_SECTION_1) $(MAN_SECTION_7) $(MAN_SECTION_8) *~

install-data-local :
	ln -sf openvpn3-config-dump.8 openvpn3-config-show.8