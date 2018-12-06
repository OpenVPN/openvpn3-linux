dnl  OpenVPN 3 Linux client -- Next generation OpenVPN client
dnl
dnl  Copyright (C) 2017-2018 OpenVPN Inc. <sales@openvpn.net>
dnl  Copyright (C) 2017-2018 David Sommerseth <davids@openvpn.net>
dnl
dnl  This program is free software: you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation, version 3 of the License
dnl
dnl  This program is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program.  If not, see <http://www.gnu.org/licenses/>.
dnl
dnl
dnl  Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
m4_include([version.m4])
AC_INIT([PRODUCT_NAME], [PRODUCT_VERSION], [PRODUCT_BUGREPORT], [PRODUCT_TARNAME])
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz tar-pax foreign -Wall -Wportability subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CXX
AC_PROG_CC
PKG_PROG_PKG_CONFIG
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_SED
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_DEFINE_UNQUOTED([PACKAGE_GUIVERSION], ["PRODUCT_GUIVERSION"], [Version string to be presented to users/logs])

dnl  This project has C++11 as the minimum C++ standard and C++14 as
dnl  recommend standard as is not expected to work with any older
dnl  standards.

AX_CXX_COMPILE_STDCXX(11, noext, [mandatory])
AX_CXX_COMPILE_STDCXX(14, noext, [optional])

dnl
dnl  This projects provide some Python based utilities, check if the
dnl  right Python version is available.  For now enforce Python 3.4 or newer.
dnl  If Python is not found, the Python modules will not be installed
dnl
AM_PATH_PYTHON([3.4],, [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])

dnl
dnl  Minor Default CXXFLAGS
dnl
CXXFLAGS="${CXXFLAGS} -Wall -Wl,--no-as-needed -Wno-unused-local-typedefs -Wno-sign-compare -Wno-unused-parameter"


dnl case "${CXXFLAGS}" in
dnl   *-std=*) ;;
dnl  *)       CXXFLAGS="${CXXFLAGS} -std=c++14" ;;
dnl esac


dnl
dnl  Check for LZ4 library
dnl
PKG_CHECK_MODULES(
        [LIBLZ4],
        [liblz4],
        [have_lz4="yes"],
        [AC_MSG_ERROR([lz4 package not found. Is the development package installed?])]
)

dnl
dnl  Check for jsoncpp library
dnl
PKG_CHECK_MODULES(
        [LIBJSONCPP],
        [jsoncpp],
        [have_jsoncpp="yes"],
        [AC_MSG_ERROR([jsoncpp package not found. Is the development package installed?])]
)

dnl
dnl  Check for glib2/gio/gdbus libraries
dnl
PKG_CHECK_MODULES(
        [LIBGLIBGIO],
        [gio-2.0],
        [have_glibgio="yes"],
        [AC_MSG_ERROR([glib2/gio package not found. Is the glib2 development package installed?])]
)

dnl
dnl  Check for UUID library
dnl
PKG_CHECK_MODULES(
        [LIBUUID],
        [uuid],
        [have_uuid="yes"],
        [AC_MSG_ERROR([libuuid package not found. Is the development package installed?])]
)

dnl
dnl  Check for libcap-ng - used for capabilities management
dnl
PKG_CHECK_MODULES(
        [LIBCAPNG],
        [libcap-ng],
        [have_libcapng="yes"],
        [AC_MSG_ERROR([libcap-ng package not found. Is the development package installed?])]
)

dnl
dnl  Check for mbed TLS library
dnl
AC_ARG_VAR([MBEDTLS_CFLAGS], [C compiler flags for mbedtls])
AC_ARG_VAR([MBEDTLS_LIBS], [linker flags for mbedtls])

saved_CFLAGS="${CFLAGS}"
saved_LIBS="${LIBS}"

if test -z "${MBEDTLS_CFLAGS}" -a -z "${MBEDTLS_LIBS}"; then
        # if the user did not explicitly specify flags, try to autodetect
        LIBS="${LIBS} -lmbedtls -lmbedx509 -lmbedcrypto"
        AC_CHECK_LIB(
                [mbedtls],
                [mbedtls_ssl_init],
                [MBEDTLS_LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"],
                [AC_MSG_ERROR([Could not find mbed TLS.])],
                [${PKCS11_HELPER_LIBS}]
        )
fi

CFLAGS="${MBEDTLS_CFLAGS} ${PKCS11_HELPER_CFLAGS} ${CFLAGS}"
LIBS="${MBEDTLS_LIBS} ${PKCS11_HELPER_LIBS} ${LIBS}"

AC_MSG_CHECKING([mbedtls version])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[
#include <mbedtls/version.h>
                ]],
                [[
#if MBEDTLS_VERSION_NUMBER < 0x02000000 || MBEDTLS_VERSION_NUMBER >= 0x03000000
#error Invalid version
#endif
                ]]
        )],
        [AC_MSG_RESULT([ok])],
        [AC_MSG_ERROR([mbed TLS 2.y.z required])]
)

have_crypto_aead_modes="yes"
AC_CHECK_FUNCS(
        [ \
                mbedtls_cipher_write_tag \
                mbedtls_cipher_check_tag \
        ],
        ,
        [have_crypto_aead_modes="no"; break]
)

CFLAGS="${saved_CFLAGS}"
LIBS="${saved_LIBS}"
have_crypto="yes"
AC_DEFINE([ENABLE_CRYPTO_MBEDTLS], [1], [Use mbed TLS library])
CRYPTO_CFLAGS="-D USE_MBEDTLS ${MBEDTLS_CFLAGS}"
CRYPTO_LIBS="${MBEDTLS_LIBS}"
AC_SUBST([CRYPTO_CFLAGS])
AC_SUBST([CRYPTO_LIBS])


dnl
dnl  Configure paths for the ASIO library and OpenVPN 3 Core library.
dnl
dnl  The default is to use the embedded ASIO library (vendor/asio/),
dnl  which is based on a specific git commit reference (managed via
dnl  git submodule).
dnl
AC_ARG_VAR([ASIO_SOURCEDIR], [Alternative ASIO library directory])
if test -z "${ASIO_SOURCEDIR}"; then
   ASIO_SOURCEDIR="`pwd`/vendor/asio"
fi
ASIO_CXXFLAGS="-I${ASIO_SOURCEDIR}/asio/include -DUSE_ASIO -DASIO_STANDALONE -DASIO_NO_DEPRECATED"
AC_SUBST([ASIO_CXXFLAGS])


dnl
dnl  Configure the default path of the OpenVPN 3 core library.
dnl
dnl  This is similar to the ASIO library, there is an embedded
dnl  version in this tree which will be used by default.  The
dnl  git checkout inside the core/ directory is managed via git
dnl  submodule.
dnl
AC_ARG_VAR([OPENVPN3_CORE_SOURCEDIR], [Alternative OpenVPN 3 core library directory])
if test -z "${OPENVPN3_CORE_SOURCEDIR}"; then
   OPENVPN3_CORE_SOURCEDIR="`pwd`/openvpn3-core"
fi
OPENVPN3_CORE_CXXFLAGS="-I${OPENVPN3_CORE_SOURCEDIR}"
AC_SUBST([OPENVPN3_CORE_CXXFLAGS])

dnl
dnl  System wide OpenVPN 3 Configuration directory
dnl
AC_ARG_VAR([OPENVPN3_CONFIGDIR], [OpenVPN 3 Linux configuration directory (default ${sysconfdir}/openvpn3)])
if test -n "${OPENVPN3_CONFIGDIR}"; then
   openvpn3_configdir="${OPENVPN3_CONFIGDIR}"
else
   openvpn3_configdir="${sysconfdir}/openvpn3"
fi
AC_SUBST([openvpn3_configdir])


dnl
dnl  systemd unit file directory
dnl
dnl  This defines the system wide directory for the systemd service
dnl  unit files should be installed
dnl
AC_ARG_VAR([SYSTEMD_UNIT_DIR], [Systemd unit file directory (default: ${prefix}/lib/systemd/system)])
if test -n "${SYSTEMD_UNIT_DIR}"; then
   systemd_unitdir="${SYSTEMD_UNIT_DIR}"
else
   systemd_unitdir="${prefix}/lib/systemd/system"
fi
AC_SUBST([systemd_unitdir])


dnl
dnl  D-Bus system policy path
dnl
dnl  This path is where the OpenVPN 3 D-Bus policy will be installed.
dnl  This policy controls which users can do which operations on
dnl  the various OpenVPN 3 D-Bus enabled services.
dnl
AC_ARG_VAR([DBUS_SYSTEM_POLICY_DIR], [D-Bus policy configuration (default: ${sysconfdir}/dbus-1/system.d)])
if test -n "${DBUS_SYSTEM_POLICY_DIR}"; then
   dbus_system_policydir="${DBUS_SYSTEM_POLICY_DIR}"
else
   dbus_system_policydir="${sysconfdir}/dbus-1/system.d"
fi
AC_SUBST([dbus_system_policydir])


dnl
dnl  D-Bus system service definition path
dnl
dnl  This path is where the OpenVPN 3 D-Bus service definitions
dnl  will be installed.  These definitions are used to auto-start
dnl  OpenVPN 3 backend services whenever they are needed.  This
dnl  also enables a priviliege separation between the end-users,
dnl  helper services and the VPN client process requiring root
dnl  privileges to start-up.
dnl
AC_ARG_VAR([DBUS_SYSTEM_SERVICE_DIR], [D-Bus service configurations (default: ${datarootdir}/dbus-1/system-services )])
if test -n "${DBUS_SYSTEM_SERVICE_DIR}"; then
   dbus_system_servicedir="${DBUS_SYSTEM_SERVICE_DIR}"
else
   dbus_system_servicedir="${datarootdir}/dbus-1/system-services"
fi
AC_SUBST([dbus_system_servicedir])


dnl
dnl  SELinux support
dnl
dnl  By default, the SELinux policy will be built if the needed
dnl  SELinux development files is found on the system.  But this can
dnl  completely disabled explicitly by providing --disable-selinux-build.
dnl  This will also remove related SELinux code paths in the various programs.
dnl
AC_ARG_ENABLE(
     [selinux-build],
     [AS_HELP_STRING([--disable-selinux-build],
                     [Disables building the SELinux policy])],
     [],
     [enable_selinux_build="yes"]
)

dnl  This path is used to detect if the SELinux policy
dnl  development files.  By default it will use /usr/share/selinux/devel
dnl
AC_ARG_VAR([SELINUX_DEVEL_PATH],
           [Directory containing the SELinux policy development files (default: ${datarootdir}/selinux/devel)])
if test -n "${SELINUX_DEVEL_PATH}"; then
        selinux_devel_path="${SELINUX_DEVEL_PATH}"
else
        AX_RECURSIVE_EVAL([${datarootdir}/selinux/devel], [selinux_devel_path])
fi
AC_SUBST([selinux_devel_path])

if test "${enable_selinux_build}" = "yes"; then
   AC_MSG_NOTICE([Checking for SELinux policy development files])
   AC_CHECK_FILE([${selinux_devel_path}/Makefile], [], [enable_selinux_build="no"])

   if test "${enable_selinux_build}" = "yes"; then
        AC_DEFINE([ENABLE_SELINUX_BUILD], [1], [Build SELinux policy and enable related code paths])
   fi
fi
AC_SUBST([ENABLE_SELINUX_BUILD])
AM_CONDITIONAL([ENABLE_SELINUX_BUILD], [test "$enable_selinux_build" = "yes"])

dnl
dnl  OpenVPN 3 Linux state directory
dnl
dnl  Defaults to ${localstatedir}/lib/openvpn3
dnl
AC_ARG_VAR([OPENVPN3_STATEDIR], [OpenVPN 3 state directory (default: ${localstatedir}/lib/openvpn3)])
if test -n "${OPENVPN3_STATEDIR}"; then
        openvpn3_statedir="${OPENVPN3_STATEDIR}"
else
        openvpn3_statedir="${localstatedir}/lib/openvpn3"
fi
AC_SUBST([openvpn3_statedir])

dnl
dnl  Path where to install the various back-end OpenVPN 3 D-Bus services
dnl
dnl  These services are most commonly not accessed directly by any end-users
dnl  but will be auto-started by the D-Bus daemon
dnl
AC_DEFINE_UNQUOTED([LIBEXEC_PATH], [LIBEXECDIR "/${PACKAGE}"], [Path where openvpn-service-* binaries resides])


dnl
dnl  User/group names for the unprivileges OpenVPN
dnl
AC_ARG_VAR(
        [OPENVPN_USERNAME],
        [Defines the username unprivileged OpenVPN services will run as @<:@default=openvpn@:>@]
)
if test -z "${OPENVPN_USERNAME}"; then
   OPENVPN_USERNAME="openvpn"
fi
AC_SUBST([OPENVPN_USERNAME])
AC_DEFINE_UNQUOTED([OPENVPN_USERNAME], ["${OPENVPN_USERNAME}"], [User group name for unprivileged operations])

AC_ARG_VAR(
        [OPENVPN_GROUP],
        [Defines the user group unprivileged OpenVPN services will run as @<:@default=openvpn@:>@]
)
if test -z "${OPENVPN_GROUP}"; then
   OPENVPN_GROUP="openvpn"
fi
AC_SUBST([OPENVPN_GROUP])
AC_DEFINE_UNQUOTED([OPENVPN_GROUP], ["${OPENVPN_GROUP}"], [User group name for unprivileged operations])


dnl
dnl  Check if rst2man is available, if so man pages will be created
dnl
AC_CHECK_PROGS([RST2MAN], [rst2man], []) # optional
AC_SUBST([HAVE_RST2MAN])
AM_CONDITIONAL([HAVE_RST2MAN], [test -n "${RST2MAN}"])

dnl
dnl  Various developer tools
dnl
AC_ARG_VAR([GIT], [path to git utility])
AC_CHECK_PROGS([GIT], [git], []) # optional
GIT_CHECKOUT="no"
if test -n "${GIT}" -a -d "${srcdir}/.git"; then
        AC_DEFINE([HAVE_CONFIG_VERSION_H], [1], [extra version available in config-version.h])
        GIT_CHECKOUT="yes"
fi
AC_MSG_RESULT([checking if this is a git checkout ... ${GIT_CHECKOUT}])
AM_CONDITIONAL([GIT_CHECKOUT], [test "${GIT_CHECKOUT}" = "yes"])


dnl
dnl  opt-in debug mode, adds arguments to some D-Bus service programs
dnl  which simplifies use of valgrind or gdb
dnl
AC_ARG_ENABLE(
     [debug-options],
     [AS_HELP_STRING([--enable-debug-options],
                     [enables debug command line options in some of the D-Bus services])],
     [enable_debug_options="yes"],
     []
)
AC_SUBST([DEBUG_OPTIONS])
if test "${enable_debug_options}" = "yes"; then
   AC_DEFINE([DEBUG_OPTIONS], [1], [add debug command line options to some D-Bus services])
fi

dnl
dnl   opt-in debug mode, will print more details when DBusExceptions happens
dnl   This used to be the default behaviour, but is now optional
dnl
AC_ARG_ENABLE(
    [debug-exceptions],
    [AS_HELP_STRING([--enable-debug-exceptions],
                    [enables more detailed DBusException messages])],
    [enable_debug_exceptions="yes"],
    []
)
AC_SUBST([DEBUG_EXCEPTIONS])
if test "${enable_debug_exceptions}" = "yes"; then
   AC_DEFINE([DEBUG_EXCEPTIONS], [1], [add more details when throwing DBusExceptions])
fi

dnl
dnl   opt-in debug mode, will log all OpenVPN 3 Core library events
dnl   happening in openvpn3-service-client as Debug log messages
dnl
AC_ARG_ENABLE(
    [debug-core-events],
    [AS_HELP_STRING([--enable-debug-core-events],
                    [enables debug logging of OpenVPN 3 Core library events])],
    [enable_debug_core_events="yes"],
    []
)
AC_SUBST([DEBUG_CORE_EVENTS])
if test "${enable_debug_core_events}" = "yes"; then
   AC_DEFINE([DEBUG_CORE_EVENTS], [1], [Debug logging of OpenVPN 3 Core library events])
fi

dnl
dnl   Make it possible to not build the various test programs by default
dnl
AC_ARG_ENABLE(
    [build-test-progs],
    [AS_HELP_STRING([--disable-build-test-progs],
                    [disable building various test programs by default])],
    ,
    [enable_build_test_progs="yes"]
)
AM_CONDITIONAL([BUILD_TESTS], [test "${enable_build_test_progs}" = "yes"])

dnl
dnl  Configuring common default autoconf/automake files
dnl
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile \
        distro/systemd/Makefile
        doxygen/Makefile \
        doxygen/openvpn3-linux.doxy \
        docs/man/Makefile \
        src/policy/Makefile \
        src/python/Makefile \
        src/selinux/Makefile \
        src/service-autostart/Makefile \
])

AC_OUTPUT
