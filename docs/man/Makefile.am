#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
#

% : %.rst Makefile
	$(AM_V_GEN)rst2man $< > $@

% : %.in
	$(AM_V_GEN)sed -e 's|\@LIBEXEC_PATH\@|$(pkglibexecdir)|' \
		       -e 's|\@OPENVPN_USERNAME\@|$(OPENVPN_USERNAME)|' \
		       -e 's|\@OPENVPN_STATEDIR\@|$(openvpn3_statedir)|' \
		       -e 's|\@DBUS_SYSTEM_POLICYDIR\@|$(dbus_system_policydir)|' \
		       -e 's|\@DBUS_SYSTEM_SERVICEDIR\@|$(dbus_system_servicedir)|' \
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
	openvpn3-admin-init-config.8 \
	openvpn3-admin-journal.8 \
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

clean-local :
	-rm -f openvpn3-config-show.8
