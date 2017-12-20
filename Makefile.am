#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
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

SUBDIRS = src/service-autostart src/policy src/tests src/utils

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

EXTRA_DIST = \
	openvpn3-core \
	vendor


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
	src/dbus/object.hpp \
	src/dbus/path.hpp \
	src/dbus/processwatch.hpp \
	src/dbus/proxy.hpp \
	src/dbus/requiresqueue-proxy.hpp \
	src/dbus/signals.hpp

#
#  User binaries
#
bin_PROGRAMS = src/ovpn3cli/openvpn3
bin_SCRIPTS =  src/ovpn3cli/openvpn2


#
#  System binaries
#
pkglibexec_PROGRAMS = \
	src/client/openvpn3-service-client \
	src/client/openvpn3-service-backendstart \
	src/configmgr/openvpn3-service-configmgr \
	src/sessionmgr/openvpn3-service-sessionmgr \
	src/log/openvpn3-service-logger

#
#  openvpn3: Command line front-end
#
src_ovpn3cli_openvpn3_SOURCES = \
	src/ovpn3cli/openvpn3.cpp \
	$(DBUS_SOURCES) \
	src/configmgr/proxy-configmgr.hpp \
	src/sessionmgr/proxy-sessionmgr.hpp \
	src/dbus/requiresqueue-proxy.hpp \
	src/common/requiresqueue.hpp \
	src/common/utils.hpp


#
#  openvpn3-service-client: The VPN client process
#
src_client_openvpn3_service_client_SOURCES = \
	src/client/openvpn3-service-client.cpp \
	src/client/core-client.hpp \
	src/client/backend-signals.hpp \
	src/client/statistics.hpp \
	$(DBUS_SOURCES) \
	src/common/core-extensions.hpp \
	src/common/requiresqueue.hpp \
	src/common/utils.hpp \
	src/configmgr/proxy-configmgr.hpp \
	src/log/dbus-log.hpp

#
#  openvpn3-service-backendstart: Service which starts VPN client processes
#
src_client_openvpn3_service_backendstart_SOURCES = \
	src/client/openvpn3-service-backendstart.cpp \
	$(DBUS_SOURCES) \
	src/common/utils.hpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-configmgr: Configuration manager
#
src_configmgr_openvpn3_service_configmgr_SOURCES = \
	src/configmgr/openvpn3-service-configmgr.cpp \
	src/configmgr/configmgr.hpp \
	$(DBUS_SOURCES) \
	src/common/core-extensions.hpp \
	src/common/utils.hpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-sessionmgr: VPN Session manager
#
src_sessionmgr_openvpn3_service_sessionmgr_SOURCES = \
	src/sessionmgr/openvpn3-service-sessionmgr.cpp \
	src/sessionmgr/sessionmgr.hpp \
	$(DBUS_SOURCES) \
	src/common/utils.hpp \
	src/log/dbus-log.hpp


#
#  openvpn3-service-logger: D-Bus based log fetcher
#
src_log_openvpn3_service_logger_SOURCES = \
	src/log/openvpn3-service-logger.cpp \
	src/log/dbus-log.hpp \
	src/log/log-helpers.hpp \
	$(DBUS_SOURCES) \
	src/common/utils.hpp

