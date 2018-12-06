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

AUTOMAKE_OPTIONS = foreign 1.9
ACLOCAL_AMFLAGS = -I m4

# Keep these environment variables when running 'make distcheck'
AM_DISTCHECK_CONFIGURE_FLAGS = OPENVPN3_CORE_SOURCEDIR="${OPENVPN3_CORE_SOURCEDIR}" ASIO_SOURCEDIR="${ASIO_SOURCEDIR}" CXXFLAGS="${CXXFLAGS}"

SUBDIRS=
if HAVE_PYTHON
SUBDIRS += src/python
endif
SUBDIRS += distro/systemd src/service-autostart src/policy doxygen docs/man

if ENABLE_SELINUX_BUILD
SUBDIRS += src/selinux
endif

MAINTAINERCLEANFILES = \
        config.log config.status \
        $(srcdir)/Makefile.in \
        $(srcdir)/config.h.in $(srcdir)/config.h.in~ $(srcdir)/configure \
        $(srcdir)/install-sh $(srcdir)/ltmain.sh $(srcdir)/missing \
        $(srcdir)/m4/libtool.m4 $(srcdir)/m4/lt~obsolete.m4 \
        $(srcdir)/m4/ltoptions.m4 $(srcdir)/m4/ltsugar.m4 \
        $(srcdir)/m4/ltversion.m4 \
        $(srcdir)/depcomp $(srcdir)/aclocal.m4 \
        $(srcdir)/config.guess $(srcdir)/config.sub

maintainer-clean-local:
	-git submodule deinit --all

CLEANFILES = \
        config-version.h *~

DOCUMENTATION = \
	COPYRIGHT.md \
	README.md \
	CLA.md \
	docs/coding-style.md \
	docs/dbus \
	docs/debugging.md

EXTRA_DIST = \
	$(DOCUMENTATION) \
	src/shell/bash_completion-openvpn3 \
	openvpn3-core \
	vendor \
	src/common/requiresqueue.hpp \
	src/common/timestamp.hpp \
	src/common/utils.hpp

# Remove openvpn3-core embedded patches which we don't need
# and makes 'make dist' complain
dist-hook:
	rm -rf $(distdir)/openvpn3-core/deps/asio/patch/*.patch

#
# Compiler flags
#
AM_CXXFLAGS = \
	-I$(srcdir)/src \
	$(OPENVPN3_CORE_CXXFLAGS) \
	$(ASIO_CXXFLAGS) \
	$(CRYPTO_CFLAGS) \
	$(LIBGLIBGIO_CFLAGS) \
	$(LIBJSONCPP_CFLAGS) \
	$(LIBLZ4_CFLAGS) \
	$(LIBUUID_CFLAGS) \
	-DLIBEXECDIR=\"$(libexecdir)\"

#
# Linker flags
#
AM_LDFLAGS = \
	$(CRYPTO_LIBS) \
	$(LIBGLIBGIO_LIBS) \
	$(LIBJSONCPP_LIBS) \
	$(LIBLZ4_LIBS) \
	$(LIBUUID_LIBS)


#
#  OpenVPN 3 specific D-Bus library
#
DBUS_SOURCES = \
	src/dbus/core.hpp \
	src/dbus/connection-creds.hpp \
	src/dbus/connection.hpp \
	src/dbus/constants.hpp \
	src/dbus/exceptions.hpp \
	src/dbus/idlecheck.hpp \
	src/dbus/glibutils.hpp \
	src/dbus/object.hpp \
	src/dbus/object-property.hpp \
	src/dbus/path.hpp \
	src/dbus/processwatch.hpp \
	src/dbus/proxy.hpp \
	src/dbus/requiresqueue-proxy.hpp \
	src/dbus/signals.hpp \
	src/dbus/glibutils.hpp

if GIT_CHECKOUT
BUILT_SOURCES = config-version.h
endif


#
#   TEST PROGRAMS
#
if BUILD_TESTS
noinst_PROGRAMS = \
	src/tests/command-parser/cmdparser \
	src/tests/ovpn3-core/profilemerge-optionlist \
	src/tests/misc/config-export-json-test \
	src/tests/misc/gettimestamp \
	src/tests/misc/json-config-import-test \
	src/tests/misc/log-prefix-selftest \
	src/tests/misc/logwriter-tests \
	src/tests/misc/lookup-tests \
	src/tests/misc/netcfg-dns-direct-file-selftest \
	src/tests/misc/syslog-facility-mapping-test \
	src/tests/dbus/config-lock-down \
	src/tests/dbus/config-override-selftest \
	src/tests/dbus/conncreds \
	src/tests/dbus/enable-logging \
	src/tests/dbus/fetch-avail-config-paths \
	src/tests/dbus/fetch-avail-session-paths \
	src/tests/dbus/fetch-config \
	src/tests/dbus/fetch-config2 \
	src/tests/dbus/get-acl \
	src/tests/dbus/get-config-overrides \
	src/tests/dbus/getlastlogevent \
	src/tests/dbus/getlaststatus \
	src/tests/dbus/getconnectionstats \
	src/tests/dbus/log-listener \
	src/tests/dbus/log-listener2 \
	src/tests/dbus/logservice1 \
	src/tests/dbus/netcfg-proxy-unit \
	src/tests/dbus/netcfg-stateevent-selftest \
	src/tests/dbus/set-alias \
	src/tests/dbus/signal-listener \
	src/tests/dbus/statusevent-selftest \
	src/tests/dbus/proc-wait-for \
	src/tests/dbus/proc-wait-for-pid \
	src/tests/dbus/proxy-checkserviceavail \
	src/tests/dbus/request-queue-client \
	src/tests/dbus/request-queue-client2 \
	src/tests/dbus/request-queue-service

# src/tests/comand-parser
#
src_tests_command_parser_cmdparser_SOURCES = \
	src/tests/command-parser/cmdparser.cpp \
	src/common/utils.cpp

# src/tests/ovpn3-core
#
src_tests_ovpn3_core_profilemerge_optionlist_SOURCES = \
	src/tests/ovpn3-core/profilemerge-optionlist.cpp \
	src/common/utils.cpp

# src/tests/misc
#
src_tests_misc_config_export_json_test_SOURCES = \
	src/tests/misc/config-export-json-test.cpp

src_tests_misc_gettimestamp_SOURCES = \
	src/tests/misc/gettimestamp.cpp \
	src/common/timestamp.cpp

src_tests_misc_json_config_import_test_SOURCES = \
	src/tests/misc/json-config-import-test.cpp

src_tests_misc_log_prefix_selftest_SOURCES = \
	src/tests/misc/log-prefix-selftest.cpp

src_tests_misc_logwriter_tests_SOURCES = \
	src/tests/misc/logwriter-tests.cpp \
	src/common/timestamp.cpp

src_tests_misc_lookup_tests_SOURCES = \
	src/tests/misc/lookup-tests.cpp \
	src/common/lookup.cpp \
	src/common/utils.cpp

src_tests_misc_netcfg_dns_direct_file_selftest_SOURCES = \
	src/tests/misc/netcfg-dns-direct-file-selftest.cpp \
	src/common/timestamp.cpp

src_tests_misc_syslog_facility_mapping_test_SOURCES = \
	src/tests/misc/syslog-facility-mapping-test.cpp

# src/tests/dbus
#
src_tests_dbus_config_lock_down_SOURCES = \
	src/tests/dbus/config-lock-down.cpp

src_tests_dbus_config_override_selftest_SOURCES = \
	src/tests/dbus/config-override-selftest.cpp

src_tests_dbus_conncreds_SOURCES = \
	src/tests/dbus/conncreds.cpp

src_tests_dbus_enable_logging_SOURCES = \
	src/tests/dbus/enable-logging.cpp

src_tests_dbus_fetch_avail_config_paths_SOURCES = \
	src/tests/dbus/fetch-avail-config-paths.cpp

src_tests_dbus_fetch_avail_session_paths_SOURCES = \
	src/tests/dbus/fetch-avail-session-paths.cpp \
	src/common/requiresqueue.cpp

src_tests_dbus_fetch_config_SOURCES = \
	src/tests/dbus/fetch-config.cpp

src_tests_dbus_fetch_config2_SOURCES = \
	src/tests/dbus/fetch-config2.cpp

src_tests_dbus_get_acl_SOURCES = \
	src/tests/dbus/get-acl.cpp \
	src/common/lookup.cpp

src_tests_dbus_get_config_overrides_SOURCES = \
	src/tests/dbus/get-config-overrides.cpp

src_tests_dbus_getlastlogevent_SOURCES = \
	src/tests/dbus/getlastlogevent.cpp

src_tests_dbus_getlaststatus_SOURCES = \
	src/tests/dbus/getlaststatus.cpp

src_tests_dbus_getconnectionstats_SOURCES = \
	src/tests/dbus/getconnectionstats.cpp

src_tests_dbus_log_listener_SOURCES = \
	src/tests/dbus/log-listener.cpp \
	src/common/utils.cpp

src_tests_dbus_log_listener2_SOURCES = \
	src/tests/dbus/log-listener2.cpp \
	src/common/utils.cpp

src_tests_dbus_logservice1_SOURCES = \
	src/tests/dbus/logservice1.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp

src_tests_dbus_netcfg_proxy_unit_SOURCES = \
	src/tests/dbus/netcfg-proxy-unit.cpp

src_tests_dbus_netcfg_stateevent_selftest_SOURCES = \
	src/tests/dbus/netcfg-stateevent-selftest.cpp

src_tests_dbus_set_alias_SOURCES = \
	src/tests/dbus/set-alias.cpp

src_tests_dbus_signal_listener_SOURCES = \
	src/tests/dbus/signal-listener.cpp \
	src/common/utils.cpp

src_tests_dbus_statusevent_selftest_SOURCES = \
	src/tests/dbus/statusevent-selftest.cpp

src_tests_dbus_proc_wait_for_SOURCES = \
	src/tests/dbus/proc-wait-for.cpp \
	src/common/utils.cpp

src_tests_dbus_proc_wait_for_pid_SOURCES = \
	src/tests/dbus/proc-wait-for-pid.cpp \
	src/common/utils.cpp

src_tests_dbus_proxy_checkserviceavail_SOURCES = \
	src/tests/dbus/proxy-checkserviceavail.cpp

src_tests_dbus_request_queue_client_SOURCES = \
	src/tests/dbus/request-queue-client.cpp \
	src/common/requiresqueue.cpp

src_tests_dbus_request_queue_client2_SOURCES = \
	src/tests/dbus/request-queue-client2.cpp \
	src/common/requiresqueue.cpp

src_tests_dbus_request_queue_service_SOURCES = \
	src/tests/dbus/request-queue-service.cpp \
	src/common/requiresqueue.cpp \
	src/common/utils.cpp

endif

#
#  User binaries
#
bin_PROGRAMS = src/ovpn3cli/openvpn3

#
#  System binaries
#
pkglibexec_PROGRAMS = \
	src/client/openvpn3-service-client \
	src/client/openvpn3-service-backendstart \
	src/configmgr/openvpn3-service-configmgr \
	src/sessionmgr/openvpn3-service-sessionmgr \
	src/netcfg/openvpn3-service-netcfg \
	src/log/openvpn3-service-logger

#
#  openvpn3: Command line front-end
#
src_ovpn3cli_openvpn3_SOURCES = \
	src/ovpn3cli/openvpn3.cpp \
	src/ovpn3cli/arghelpers.hpp \
	src/common/lookup.cpp \
	src/common/lookup.hpp \
	src/ovpn3cli/commands/config.hpp \
	src/ovpn3cli/commands/log.hpp \
	src/ovpn3cli/commands/session.hpp \
	$(DBUS_SOURCES) \
	src/configmgr/proxy-configmgr.hpp \
	src/configmgr/overrides.hpp \
	src/sessionmgr/proxy-sessionmgr.hpp \
	src/dbus/requiresqueue-proxy.hpp \
	src/common/cmdargparser.hpp \
	src/common/requiresqueue.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp

#
#  openvpn3-service-client: The VPN client process
#
src_client_openvpn3_service_client_SOURCES = \
	src/client/openvpn3-service-client.cpp \
	src/client/core-client.hpp \
	src/client/core-client-netcfg.hpp \
	src/client/backend-signals.hpp \
	src/client/statistics.hpp \
	src/client/statusevent.hpp \
	$(DBUS_SOURCES) \
	src/common/core-extensions.hpp \
	src/common/requiresqueue.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp \
	src/configmgr/proxy-configmgr.hpp \
	src/log/dbus-log.hpp \
	src/log/proxy-log.hpp \
	src/netcfg/proxy-netcfg.hpp

#
#  openvpn3-service-backendstart: Service which starts VPN client processes
#
src_client_openvpn3_service_backendstart_SOURCES = \
	src/client/openvpn3-service-backendstart.cpp \
	$(DBUS_SOURCES) \
	src/common/utils.cpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-configmgr: Configuration manager
#
src_configmgr_openvpn3_service_configmgr_SOURCES = \
	src/configmgr/openvpn3-service-configmgr.cpp \
	src/configmgr/configmgr.hpp \
	$(DBUS_SOURCES) \
	src/common/core-extensions.hpp \
	src/common/lookup.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-sessionmgr: VPN Session manager
#
src_sessionmgr_openvpn3_service_sessionmgr_SOURCES = \
	src/sessionmgr/openvpn3-service-sessionmgr.cpp \
	src/sessionmgr/sessionmgr.hpp \
	src/client/statusevent.hpp \
	$(DBUS_SOURCES) \
	src/common/lookup.cpp \
	src/common/requiresqueue.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-netcfg: Network Configuration Manager
#
src_netcfg_openvpn3_service_netcfg_SOURCES = \
	src/netcfg/openvpn3-service-netcfg.cpp \
	src/netcfg/core-tunbuilder.cpp \
	src/netcfg/netcfg.hpp \
	src/netcfg/netcfg-device.hpp \
	src/netcfg/netcfg-exception.hpp \
	src/netcfg/netcfg-options.hpp \
	src/netcfg/netcfg-signals.hpp \
	src/netcfg/netcfg-stateevent.hpp \
	src/netcfg/core-tunbuilder.hpp \
	src/netcfg/dns-direct-file.hpp \
	src/netcfg/dns-resolver-settings.hpp \
	$(DBUS_SOURCES) \
	src/common/lookup.cpp \
	src/common/requiresqueue.cpp \
	src/common/timestamp.cpp \
	src/common/utils.cpp \
	src/log/core-dbus-logbase.hpp \
	src/log/dbus-log.hpp
src_netcfg_openvpn3_service_netcfg_CXXFLAGS = $(LIBCAPNG_CFLAGS) $(AM_CXXFLAGS)
src_netcfg_openvpn3_service_netcfg_LDFLAGS = $(LIBCAPNG_LIBS) $(AM_LDFLAGS)


#
#  openvpn3-service-logger: D-Bus based log fetcher
#
src_log_openvpn3_service_logger_SOURCES = \
	src/log/openvpn3-service-logger.cpp \
	src/log/ansicolours.hpp \
	src/log/colourengine.hpp \
	src/log/dbus-log.hpp \
	src/log/log-helpers.hpp \
	src/log/logevent.hpp \
	src/log/logger.hpp \
	src/log/logwriter.hpp \
	src/log/service.hpp \
	src/common/timestamp.cpp \
	$(DBUS_SOURCES) \
	src/common/utils.cpp

#
#  Track git referenes of the check-out
#
config-version.h:
	@CONFIGURE_GIT_CHFILES="`GIT_DIR=\"$(top_srcdir)/.git\" $(GIT) diff-files --name-status -r --ignore-submodules --quiet -- || echo \"+\"`"; \
	CONFIGURE_GIT_UNCOMMITTED="`GIT_DIR=\"$(top_srcdir)/.git\" $(GIT) diff-index --cached  --quiet --ignore-submodules HEAD || echo \"*\"`"; \
	CONFIGURE_GIT_REVISION="`GIT_DIR=\"$(top_srcdir)/.git\" $(GIT) rev-parse --symbolic-full-name HEAD | cut -d/ -f3-`:`GIT_DIR=\"$(top_srcdir)/.git\" $(GIT) rev-parse --short=16 HEAD`"; \
	OPENVPN_VERSION="`$(top_srcdir)/openvpn3-core/scripts/version`"; \
	echo "#define CONFIGURE_GIT_REVISION \"$${CONFIGURE_GIT_REVISION}\"" > config-version.h.tmp; \
	echo "#define CONFIGURE_GIT_FLAGS \"$${CONFIGURE_GIT_CHFILES}$${CONFIGURE_GIT_UNCOMMITTED}\"" >> config-version.h.tmp; \
	echo "#define OPENVPN_VERSION \"$${OPENVPN_VERSION}\"" >> config-version.h.tmp

	@if ! [ -f config-version.h ] || ! cmp -s config-version.h.tmp config-version.h; then \
		echo "replacing config-version.h"; \
		mv config-version.h.tmp config-version.h; \
	else \
		rm -f config-version.h.tmp; \
	fi
