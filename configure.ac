dnl  OpenVPN 3 Linux client -- Next generation OpenVPN client
dnl
dnl  Copyright (C) 2017 - 2022  OpenVPN Inc. <sales@openvpn.net>
dnl  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz tar-pax foreign subdir-objects -Wall -Wno-portability])
AM_SILENT_RULES([yes])
AC_PROG_CXX
AC_PROG_CC
PKG_PROG_PKG_CONFIG
AC_PROG_CPP
AC_PROG_RANLIB
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
AM_PATH_PYTHON([3.5],, [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])
AC_DEFINE_UNQUOTED([PYTHON_VERSION], ["${PYTHON_VERSION}"], [Python version found during build])

dnl
dnl  Minor Default CXXFLAGS
dnl
CXXFLAGS="${CXXFLAGS} -Wall"

dnl
dnl  Check for LZ4 library
dnl
PKG_CHECK_MODULES(
        [LIBLZ4],
        [liblz4],
        [have_lz4="yes"],
        [AC_MSG_ERROR([lz4 package not found. Is the development package installed?])]
)
AC_SUBST([HAVE_LZ4])
if test "$have_lz4" = "yes"; then
        AC_DEFINE([HAVE_LZ4], [1], [liblz4 is found as a system package])
fi

dnl
dnl  Check for jsoncpp library
dnl
PKG_CHECK_MODULES(
        [LIBJSONCPP],
        [jsoncpp],
        [AC_DEFINE_UNQUOTED([HAVE_JSONCPP], [1], [Using jsoncpp library])],
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

PKG_CHECK_MODULES(
        [LIBGLIBGIOUNIX],
        [gio-unix-2.0],
        [have_glibgiounix="yes"],
        [AC_MSG_ERROR([glib2/gio-unix package not found. Is the glib2 development package installed?])]
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
dnl  Check for systemd libraries; used for an alternative MachineID implementation
dnl
PKG_CHECK_MODULES(
        [LIBSYSTEMD],
        [libsystemd > 218],
        [have_libsystemd="yes"],
        []
)
if test "$have_libsystemd" = "yes"; then
        saved_CFLAGS="${CFLAGS}"
        saved_LIBS="${LIBS}"
        CFLAGS="${CFLAGS} ${LIBSYSTEMD_CFLAGS}"
        LIBS="${LIBS} ${LIBSYSTEMD_LIBS}"
        AC_CHECK_FUNCS([sd_id128_get_machine],
                       [AC_CHECK_FUNCS([sd_id128_to_string],
                                       [have_systemd_libs="yes"],
                                       [])
                       ],
                       []
                      )
fi
AC_SUBST([HAVE_SYSTEMD])
if test "$have_systemd_libs" = "yes"; then
        AC_DEFINE([HAVE_SYSTEMD], [1],
                  [Needed systemd library functions are available]
        )
fi;


PKG_CHECK_MODULES(
        [LIBTINYXML2],
        [tinyxml2],
        [
            AC_LANG([C++])
            LIBS="${LIBS} ${TINYXML2_LIBS}"
            have_tinyxml="yes"
            AC_MSG_CHECKING([presence of XMLDocument::ErrorName])
            AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM(
                    [[
#include <tinyxml2.h>
                    ]],
                    [[
tinyxml2::XMLDocument doc;
doc.ErrorName();
                    ]]
                )],
                [
                    AC_MSG_RESULT([yes])
                    AC_DEFINE([OVPN_TINYXML2_HAS_ERROR_NAME], [1], [TinyXML2 has XMLDocument::ErrorName method])
                ],
                [
                    AC_MSG_RESULT([no])
                ]
            )

            AC_MSG_CHECKING([presence of XMLDocument::ErrorStr])
            AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM(
                    [[
#include <tinyxml2.h>
                    ]],
                    [[
tinyxml2::XMLDocument doc;
doc.ErrorStr();
                    ]]
                )],
                [
                    AC_MSG_RESULT([yes])
                    AC_DEFINE([OVPN_TINYXML2_HAS_ERROR_STR], [1], [TinyXML2 has XMLDocument::ErrorStr method])
                ],
                [
                    AC_MSG_RESULT([no])
                ]
            )
          ],
          [AC_MSG_ERROR([tinyxml2 package not found.])]
)
if test "$have_tinyxml" = "yes"; then
        AC_DEFINE([HAVE_TINYXML], [1], [tinyxml library is available])
fi
AM_CONDITIONAL([HAVE_TINYXML], [test "${have_tinyxml}" = "yes"])


dnl
dnl  Provide facility to switch between SSL/TLS library
dnl
AC_ARG_WITH(
        [crypto-library],
        [AS_HELP_STRING([--with-crypto-library=LIB], [build using the given crypto library backend. Valid options: mbedtls, openssl (default)])],
        [
                case "${withval}" in
                        mbedtls|openssl)
                                ;;
                        *)
                                AC_MSG_ERROR([Invalid crypto library: ${withval}])
                                ;;
                esac
        ],
        [with_crypto_library="openssl"]
)


dnl
dnl  Check for mbed TLS library
dnl
AC_ARG_VAR([MBEDTLS_CFLAGS], [C compiler flags for mbedtls])
AC_ARG_VAR([MBEDTLS_LIBS], [linker flags for mbedtls])

if test "${with_crypto_library}" = "mbedtls"; then
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
fi


dnl
dnl  Check for OpenSSL library
dnl
AC_ARG_VAR([OPENSSL_CFLAGS], [C compiler flags for OpenSSL])
AC_ARG_VAR([OPENSSL_LIBS], [linker flags for OpenSSL])

if test "${with_crypto_library}" = "openssl"; then
        saved_CFLAGS="${CFLAGS}"
        saved_LIBS="${LIBS}"

        if test -z "${OPENSSL_CFLAGS}" -a -z "${OPENSSL_LIBS}"; then
                PKG_CHECK_MODULES(
                        [OPENSSL],
                        [openssl >= 1.0.2],
                        [have_openssl="yes"],
                        [] # This will be double checked a bit later
                )
                OPENSSL_LIBS=${OPENSSL_LIBS:--lssl -lcrypto}
        fi

       CFLAGS="${CFLAGS} ${OPENSSL_CFLAGS}"
       LIBS="${LIBS} ${OPENSSL_LIBS}"

       # If pkgconfig check failed or OPENSSL_CFLAGS/OPENSSL_LIBS env vars
       # are used, check the version directly in the OpenSSL include file
       if test "${have_openssl}" != "yes"; then
               AC_MSG_CHECKING([additionally if OpenSSL is available and version >= 1.0.2])
               AC_COMPILE_IFELSE(
                       [AC_LANG_PROGRAM(
                               [[
#include <openssl/opensslv.h>
                               ]],
                               [[
/*          Version encoding: MNNFFPPS - see opensslv.h for details */
#if OPENSSL_VERSION_NUMBER < 0x10002000L
#error OpenSSL too old
#endif
                               ]]
                       )],
                       [AC_MSG_RESULT([ok])],
                       [AC_MSG_ERROR([OpenSSL 1.0.2 or newer is required])]
               )
       fi

        CFLAGS="${saved_CFLAGS}"
        LIBS="${saved_LIBS}"
        CRYPTO_CFLAGS="-D USE_OPENSSL ${OPENSSL_CFLAGS}"
        CRYPTO_LIBS="${OPENSSL_LIBS}"
fi


dnl
dnl   Export the crypto cflags and libs settings discovered
dnl
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
    ASIO_SOURCEDIR="${srcdir}/vendor/asio"
fi
AC_MSG_NOTICE([Using ASIO source directory: ${ASIO_SOURCEDIR}])
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
AC_ARG_VAR([OPENVPN3_CORE_SOURCEDIR], [Alternative OpenVPN 3 Core Library directory])
if test -z "${OPENVPN3_CORE_SOURCEDIR}"; then
   OPENVPN3_CORE_SOURCEDIR="${srcdir}/openvpn3-core"
fi
AC_MSG_NOTICE([Using OpenVPN 3 Core Library directory: ${OPENVPN3_CORE_SOURCEDIR}])
AC_MSG_CHECKING([OpenVPN 3 Core Library version])
if test -e "${srcdir}/.git"; then
   OPENVPN3_CORE_VERSION="`${OPENVPN3_CORE_SOURCEDIR}/scripts/version`"
elif test -f ${srcdir}/openvpn3-core-version; then
   OPENVPN3_CORE_VERSION="`cat ${srcdir}/openvpn3-core-version`"
else
   AC_MSG_ERROR([Missing openvpn3-core version information. Corrupt package?])
fi
AC_MSG_RESULT([${OPENVPN3_CORE_VERSION}])
OPENVPN3_CORE_CXXFLAGS="-I${OPENVPN3_CORE_SOURCEDIR} -DOPENVPN_VERSION=\\\"${OPENVPN3_CORE_VERSION}\\\""
AC_SUBST([OPENVPN3_CORE_CXXFLAGS])

dnl
dnl  Make sure we use SITNL
dnl
AC_DEFINE_UNQUOTED([OPENVPN_USE_SITNL], [1], [Using sitnl (wrapper over netlink) to manipulate network settings])

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
dnl The default $prefix is set to 'NONE' but due to
dnl some odd autoconf behaviour, $prefix is never
dnl changed from 'NONE' unless --prefix is given.
dnl
dnl Enforce $prefix to the autoconf default prefix
dnl if --prefix has not been given on the command line.
dnl
test "x$prefix" = xNONE && prefix=$ac_default_prefix

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
dnl  Default DNS resolver
dnl
AC_ARG_VAR([DEFAULT_DNS_RESOLVER], [Default DNS resolver configuration (default: --resolv-conf /etc/resolv.conf)])
if test -n "${DEFAULT_DNS_RESOLVER}"; then
   dns_resolver_config="${DEFAULT_DNS_RESOLVER}"
else
   if test -r /etc/os-release; then
      AC_MSG_NOTICE([Found /etc/os-release ... parsing it])
      SAVED_VERSION="${VERSION}"
      . /etc/os-release
      AC_MSG_NOTICE([Operating system: ${NAME} ${VERSION}])
      if test "${ID}" == "ubuntu"; then
         major_ver="`echo ${VERSION_ID} | cut -d. -f1`"
         if test ${major_ver} -ge 20; then
             dns_resolver_config="--systemd-resolved"
         fi
      elif test "${ID}" == "fedora"; then
         dns_resolver_config="--systemd-resolved"
      fi
      VERSION="${SAVED_VERSION}"
   fi
   if test -z "${dns_resolver_config}"; then
      dns_resolver_config="--resolv-conf /etc/resolv.conf"
   fi
fi
AC_MSG_NOTICE([Default DNS resolver configuration: ${dns_resolver_config}])
AC_SUBST([dns_resolver_config])

dnl
dnl  Enables support for the ovpn-dco kernel module data channel offload (DCO)
dnl
dnl  When this option is enabled, OpenVPN 3 will try to use the ovpn-dco kernel
dnl  module to offload data channel operations to the kernel.
dnl
AC_ARG_ENABLE(
    [dco],
    [AS_HELP_STRING([--enable-dco],
                    [enable building support for ovpn-dco kernel module offload])],
    [enable_dco="$enableval"],
    [enable_dco="no"]
)

dnl
dnl  Configure path for the ovpn-dco kernel module source directory.
dnl
dnl  This is similar to the core librariy, there is an embedded
dnl  version in this tree which will be used by default.  The
dnl  git checkout inside the ovpn-dco/ directory is managed via git
dnl  submodule.
dnl
AC_ARG_VAR([DCO_SOURCEDIR], [Alternative ovpn-dco kernel module source directory])
if test -z "${DCO_SOURCEDIR}"; then
    DCO_SOURCEDIR="${srcdir}/ovpn-dco"
fi
AC_MSG_NOTICE([Using ovpn-dco source directory: ${DCO_SOURCEDIR}])
AC_SUBST([DCO_SOURCEDIR])

dnl
dnl Include generic netlink library used by core library code which talks to ovpn-dco
dnl
if test "$enable_dco" = "yes"; then
    PKG_CHECK_MODULES(
        [LIBNL_GENL],
        [libnl-genl-3.0 >= 3.2.29],
        [have_libnl="yes"],
        [AC_MSG_ERROR([libnl-genl-3.0 package not found or too old. Is the development package installed? Must be version 3.4.0 or newer])]
    )
    PKG_CHECK_MODULES(
        [PROTOBUF],
        [protobuf >= 2.4.0],
        [],
        [AC_MSG_ERROR([protobuf package not found. Is the development package installed?])]
    )
    AC_SUBST(PROTOBUF_LIBS)

    DCO_CXXFLAGS="-I${DCO_SOURCEDIR}/include/uapi -DENABLE_OVPNDCO ${LIBNL_GENL_CFLAGS}"
    AC_CHECK_PROG([PROTOC], [protoc], [protoc])
    if test -z "${PROTOC}"; then
       AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])
    fi
else
    DCO_CXXFLAGS=""
fi
AM_CONDITIONAL([ENABLE_OVPNDCO], [test "${enable_dco}" = "yes"])
AC_SUBST([DCO_CXXFLAGS])

dnl
dnl  D-Bus system policy path
dnl
dnl  This path is where the OpenVPN 3 D-Bus policy will be installed.
dnl  This policy controls which users can do which operations on
dnl  the various OpenVPN 3 D-Bus enabled services.
dnl
AC_ARG_VAR([DBUS_SYSTEM_POLICY_DIR], [D-Bus policy configuration (default: ${datadir}/dbus-1/system.d)])
if test -n "${DBUS_SYSTEM_POLICY_DIR}"; then
   dbus_system_policydir="${DBUS_SYSTEM_POLICY_DIR}"
else
   dbus_system_policydir="${datadir}/dbus-1/system.d"
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
dnl  Polkit - Policy rules
dnl
AC_ARG_VAR([POLKIT_RULES_DIR], [Polkit rules directory (default: ${datarootdir}/polkit-1/rules.d )])
if test -n "${POLKIT_RULES_DIR}"; then
   polkit_rulesdir="${POLKIT_RULES_DIR}"
else
   polkit_rulesdir="${datarootdir}/polkit-1/rules.d"
fi
AC_SUBST([polkit_rulesdir])

dnl
dnl  Polkit - Legacy Local Authority policy files
dnl           This policy is only needed on Debian/Ubuntu which ships an old
dnl           polkit release which does not support the new .rules approach
dnl

AC_ARG_VAR([POLKIT_PKLA_DIR], [Legacy Polkit PKLA directory (default:  ${localstatedir}/lib/polkit-1/localauthority/10-vendor.d/ )])
if test -n "${POLKIT_PKLA_DIR}"; then
   polkit_pkladir="${POLKIT_PKLA_DIR}"
else
   polkit_pkladir="${localstatedir}/lib/polkit-1/localauthority/10-vendor.d/"
fi
AC_SUBST([polkit_pkladir])

AC_ARG_ENABLE(
    [legacy-polkit-pkla],
    [AS_HELP_STRING([--enable-legacy-polkit-pkla],
                    [Installs the legacy PKLA based polkit local authority policy files])],
    [enable_polkit_pkla="yes"],
    []
)
AM_CONDITIONAL([ENABLE_POLKIT_PKLA], [test "$enable_polkit_pkla" = "yes"])
AC_SUBST([ENABLE_POLKIT_PKLA])

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
if test "${enable_selinux_build}" = "yes"; then
   AC_ARG_VAR([SELINUX_DEVEL_PATH],
           [Directory containing the SELinux policy development files (default: ${datarootdir}/selinux/devel)])
   if test -n "${SELINUX_DEVEL_PATH}"; then
        selinux_devel_path="${SELINUX_DEVEL_PATH}"
   else
        AX_RECURSIVE_EVAL([${datarootdir}/selinux/devel], [selinux_devel_path])
   fi
   AC_MSG_NOTICE([Checking for SELinux policy development files])
   AC_CHECK_FILE([${selinux_devel_path}/Makefile], [],
                 [AC_CHECK_FILE([/usr/share/selinux/devel],
                                [selinux_devel_path="/usr/share/selinux/devel"],
                                [enable_selinux_build="no"])])

   AC_SUBST([selinux_devel_path])


   if test "${enable_selinux_build}" = "yes"; then
        AC_DEFINE([ENABLE_SELINUX_BUILD], [1], [Build SELinux policy and enable related code paths])
   fi
fi
AC_SUBST([ENABLE_SELINUX_BUILD])
AM_CONDITIONAL([ENABLE_SELINUX_BUILD], [test "$enable_selinux_build" = "yes"])


dnl
dnl  bash-completion support
dnl
dnl  Enabling this will also install the shell-completion scripts
dnl
AC_ARG_ENABLE(
     [bash-completion],
     [AS_HELP_STRING([--enable-bash-completion],
                     [Enable installing bash-completion scripts])],
     [enable_bash_completion="yes"]
)
AC_SUBST([ENABLE_BASH_COMPLETION])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION], [test "${enable_bash_completion}" = "yes"])

AC_ARG_VAR([BASH_COMPLETION_DIR], [Installation directory for bash-completion scripts (default: ${datarootdir}/bash-completion)])
if test -n "${BASH_COMPLETION_DIR}"; then
        bash_completion_dir="${BASH_COMPLETION_DIR}"
else
        bash_completion_dir="${datarootdir}/bash-completion/completions"
fi
AC_SUBST([bash_completion_dir])


dnl
dnl  Add-ons: AWS VPC support
dnl
dnl  This enables building of AWS addon, which enables communication
dnl  between VPC and VPN networks.
dnl
AC_ARG_ENABLE(
     [addons-aws],
     [AS_HELP_STRING([--enable-addons-aws],
                     [Enable building AWS add-on])],
     [enable_addons_aws="yes"]
)
if test "${enable_addons_aws}" = "yes" && test "${with_crypto_library}" != "openssl"; then
     AC_MSG_ERROR([The AWS integration requires OpenSSL])
fi
AM_CONDITIONAL([BUILD_ADDONS_AWS], [test "${enable_addons_aws}" = "yes"])


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
AC_DEFINE_UNQUOTED([OPENVPN3_STATEDIR],["${openvpn3_statedir}"], [Path to OpenVPN 3 state directory])


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
if test -n "${GIT}" -a -e "${srcdir}/.git"; then
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
AC_SUBST([OPENVPN_DEBUG])
if test "${enable_debug_options}" = "yes"; then
   AC_DEFINE([OPENVPN_DEBUG], [1], [add debug command line options to some D-Bus services])
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
dnl   opt-in debug mode, will add additional logging to the Linux kernel
dnl   NETLINK API calls
dnl
AC_ARG_ENABLE(
    [debug-netcfg-netlink],
    [AS_HELP_STRING([--enable-debug-netcfg-netlink],
                    [enables debug logging of OpenVPN 3 Core NETLINK calls])],
    [enable_debug_netcfg_netlink="yes"],
    []
)
AC_SUBST([DEBUG_RTNL])
if test "${enable_debug_netcfg_netlink}" = "yes"; then
   AC_DEFINE([DEBUG_RTNL], [1], [Debug logging of OpenVPN 3 Core NETLINK calls])
fi

dnl
dnl   Debug feature - disables the Session Manager check in backend client
dnl
dnl   Disabling this feature is a highly security senstive change and will
dnl   allow other D-Bus clients to manipulate backend VPN clients.  This
dnl   feature requires OPENVPN_DEBUG to be enabled.
dnl
AC_ARG_ENABLE(
    [debug-disable-sessionmgr-check],
    [AS_HELP_STRING([--enable-debug-disable-sessionmgr-check],
                    [WARNING - SECURITY SENSITIVE: Will disable the check in openvpn3-service-client restricting D-Bus call to come from the session manager])],
    [debug_disable_sessmgr_check="yes"],
    []
)
if test "${enable_debug_options}" = "yes" -a "${debug_disable_sessmgr_check}" = "yes"; then
    AC_DEFINE([DEBUG_DISABLE_SESSIONMGR_CHECK], [1], [WARNING - SECURITY SENSITIVE: Session manager check disabled in openvpn3-service-client])
fi

dnl
dnl  Build googletest/gtest based unit-tests
dnl
AC_ARG_ENABLE(
     [unit-tests],
     [AS_HELP_STRING([--disable-unit-tests],
                     [Disables building and running the unit tests suite])],
     [],
     [enable_unit_tests="yes"]
)
AM_CONDITIONAL([ENABLE_UNITTESTS], [test "${enable_unit_tests}" = "yes" ])
AC_SUBST([ENABLE_UNITTESTS])

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
AM_CONDITIONAL([BUILD_TEST_PROGS], [test "${enable_build_test_progs}" = "yes"])

dnl
dnl  Configuring common default autoconf/automake files
dnl
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile \
        distro/systemd/Makefile \
        doxygen/Makefile \
        doxygen/openvpn3-linux.doxy \
        docs/man/Makefile \
        src/policy/Makefile \
        src/python/Makefile \
        src/selinux/Makefile \
        src/service-autostart/Makefile \
        addons/aws/Makefile
])
AC_CONFIG_LINKS([src/policy/syntax-check.sh:src/policy/syntax-check.sh])
AC_OUTPUT
