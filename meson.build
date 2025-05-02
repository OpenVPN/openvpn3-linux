#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

project(
        'openvpn3-linux','cpp',
        default_options : [ 'cpp_std=c++17', 'werror=true' ],
        license: 'AGPL-3.0-only',
        version: run_command(['scripts/get-version','--prod-version'], check: true).stdout().strip(),
        meson_version: '>= 0.58'
)

compiler = meson.get_compiler('cpp')
if compiler.get_id() == 'gcc' and compiler.version().version_compare('<10.0')
    message('GCC compiler version < 10 found, adding "-lstdc++fs" linking flag')
    add_project_link_arguments(['-lstdc++fs'], language: 'cpp')
endif

#  Dependencies
dep_lz4 = dependency('liblz4', required: false)
dep_selinux = dependency('libselinux', required: false)
dep_systemd = dependency('libsystemd', version: '>218', required: false)
dep_tinyxml2 = dependency('tinyxml2')
dep_jsoncpp = dependency('jsoncpp')

# This is not a link dependency for this project, but we need it
# to get the proper paths for policy and service config installation
dep_dbus = dependency('dbus-1')

base_dependencies = [
    dep_jsoncpp,
    dep_lz4,
    dep_selinux,
    dep_systemd,
    dep_tinyxml2,
    dependency('gdbuspp'),
    dependency('glib-2.0'),
    dependency('gio-2.0'),
    dependency('gio-unix-2.0'),
    dependency('libcap-ng'),
    dependency('libnl-3.0'),
    dependency('openssl', version: '>= 1.0.2'),
    dependency('threads'),
    dependency('uuid'),
]


# Check Python version
pymod = import('python')
python = pymod.find_installation('python3', required: false)
if python.found()
    have_python = python.language_version().version_compare('>= 3.6')
    message('Python 3 version: ' + python.language_version())
else
    message('Python 3 version: (not found)')
endif

#
# Various additional checks of dependencies
#
compiler = meson.get_compiler('cpp')

# Check for the sd_id128_to_string() method in libsystemd
sd_id128 = false
if (dep_systemd.found())
    sd_id128 = compiler.has_function('sd_id128_to_string',
                                     prefix: '#include <systemd/sd-id128.h>',
                                     dependencies: dep_systemd)
endif

#
#  Setup additional include header dirs
#
asio_inc = get_option('asio_path') / 'include'
message ('ASIO library: ' + asio_inc)

openvpn3_core_inc = get_option('openvpn3_core_path')
message('OpenVPN 3 Core Library: ' + openvpn3_core_inc)

include_dirs = include_directories([
    openvpn3_core_inc,
    asio_inc,
    'src',
])

add_project_arguments('-Wno-non-virtual-dtor',
                      language: 'cpp')

#
# Various other settings
#
openvpn3_statedir = get_option('openvpn3_statedir')
if openvpn3_statedir == ''
    openvpn3_statedir = get_option('sharedstatedir') / 'openvpn3'
endif

devposture_profiledir = get_option('devposture_profiledir')
if devposture_profiledir == ''
    devposture_profiledir = get_option('sharedstatedir') + '/openvpn3/deviceposture/profiles'
endif

docdir = get_option('docdir')
if docdir == ''
    docdir = get_option('prefix') / get_option('datadir') / 'doc' / 'openvpn3'
endif
message('Documentation install directory: ' + docdir)

#
#  Create a build configuration header file
#
libexec_dir = get_option('libexecdir') / meson.project_name()

dco_enabled = get_option('dco').auto() or get_option('dco').enabled()

build_config = configuration_data({
    'OPENVPN_DEBUG': get_option('debug_options') or get_option('debug'),
    'DEBUG_EXCEPTIONS': get_option('debug_exceptions') or get_option('debug'),
    'DEBUG_CORE_EVENTS': get_option('debug_core_events'),
    'DEBUG_RTNL': get_option('debug_netcfg_netlink'),
    'DEBUG_DISABLE_SECURITY_CHECKS': get_option('debug_disable_security_checks'),
    'ENABLE_OVPNDCO': dco_enabled,
    'HAVE_LZ4': dep_lz4.found(),
    'HAVE_JSONCPP': dep_jsoncpp.found(),
    'HAVE_SELINUX': dep_selinux.found(),
    'HAVE_SYSTEMD': sd_id128,
    'HAVE_TINYXML': dep_tinyxml2.found(),
    #'OVPN_TINYXML2_HAS_ERROR_NAME': dep_tinyxml2.version().version_compare('>= 4.0.0'),
    #'OVPN_TINYXML2_HAS_ERROR_STR': dep_tinyxml2.version().version_compare('>= 6.0.0'),
    'ASIO_STANDALONE': true,
    'ASIO_NO_DEPRECATED': true,
    'USE_ASIO': true,
    'USE_OPENSSL': true,
    'USE_TUN_BUILDER': true,
    'OPENVPN_USE_SITNL': true,
})

build_config.set_quoted('LIBEXEC_PATH',
                        get_option('prefix') / libexec_dir,
                        description: 'Path where all the D-Bus services are installed')
build_config.set_quoted('OPENVPN_USERNAME', get_option('openvpn_username'),
                        description: 'Default OpenVPN username owning processes and files')
build_config.set_quoted('OPENVPN_GROUP', get_option('openvpn_group'),
                        description: 'Default OpenVPN group name owning processes and files')
build_config.set_quoted('OPENVPN3_STATEDIR', openvpn3_statedir,
                        description: 'System wide data directory for OpenVPN 3 Linux')

configure_file(output : 'build-config.h',
    configuration: build_config
)

# Hack until OpenVPN 3 Core library is fixed
if (dep_tinyxml2.version().version_compare('>= 4.0.0'))
    add_project_arguments('-DOVPN_TINYXML2_HAS_ERROR_NAME', language:'cpp')
    if (dep_tinyxml2.version().version_compare('>= 6.0.0'))
        add_project_arguments('-DOVPN_TINYXML2_HAS_ERROR_STR', language:'cpp')
    endif
endif

# Prepare some version details - if on a git checkout
# For tarball builds, the build-version.h should already pre-exist
# in ./src
fs = import('fs')
if fs.exists(meson.project_source_root() / '.git')
    openvpn3_core_ver = get_option('openvpn3_core_version')
    if openvpn3_core_ver == ''
       openvpn3_core_ver = run_command(['scripts/get-version','--core-version'], check: true).stdout().strip()
    endif

    version_header = configuration_data()
    version_header.set_quoted('OPENVPN_VERSION',
                              openvpn3_core_ver,
                              description: 'OpenVPN 3 Core Library version')
    version_header.set_quoted('PACKAGE_GUIVERSION',
                              run_command(['scripts/get-version','--gui-version'], check: true).stdout().strip(),
                              description: 'OpenVPN 3 Linux version identifier')
    version_header.set_quoted('PACKAGE_NAME', 'OpenVPN3/Linux',
                              description: 'Name of this project')
    configure_file(
        configuration: version_header,
        output: 'build-version.h'
    )
endif
meson.add_dist_script(find_program('scripts/prepare-build-version-h'))
cp_prog = find_program('cp', required: true)

# Unless test_programs are not explicitly enabled,
# only build them in debug builds
build_test_programs = get_option('test_programs').enabled()
if (get_option('test_programs').auto())
    build_test_programs = get_option('debug')
endif

build_unit_tests = get_option('unit_tests').enabled()
if (get_option('unit_tests').auto())
    build_unit_tests = not get_option('test_programs').disabled()
endif

message('OpenVPN 3 Linux service binary directory: ' + get_option('prefix') / libexec_dir)

#
#  D-Bus configuration
dbus_policy_dir = get_option('dbus_policy_dir')
if dbus_policy_dir == ''
    dbus_policy_dir = dep_dbus.get_variable('datadir') / 'dbus-1' / 'system.d'
endif

dbus_service_dir = get_option('dbus_system_service_dir')
if dbus_service_dir == ''
    dbus_service_dir = dep_dbus.get_variable('system_bus_services_dir')
endif

dbus_config = {
    'OPENVPN_USERNAME': get_option('openvpn_username'),
    'LIBEXEC_PATH': get_option('prefix') / libexec_dir,
    'DBUS_SYSTEM_POLICYDIR': dbus_policy_dir,
    'DBUS_SYSTEM_SERVICEDIR': dbus_service_dir,
}
message('D-Bus system bus services dir: ' + dbus_service_dir)
message('D-Bus system policy dir: ' + dbus_policy_dir)

subdir('src/dco')

#
#  Shared code among more components
#  Build them as a static library and link against it
#
common_code = static_library(
        'common',
        [
            'src/common/cmdargparser.cpp',
            'src/common/configfileparser.cpp',
            'src/common/lookup.cpp',
            'src/common/machineid.cpp',
            'src/common/open-uri.cpp',
            'src/common/platforminfo.cpp',
            'src/common/requiresqueue.cpp',
            'src/common/string-utils.cpp',
            'src/common/timestamp.cpp',
            'src/common/utils.cpp',
            'src/dbus/object-ownership.cpp',
            'src/dbus/path.cpp',
            'src/events/attention-req.cpp',
            'src/events/log.cpp',
            'src/events/status.cpp',
            'src/log/core-dbus-logger.cpp',
            'src/log/dbus-log.cpp',
            'src/log/journal-log-parse.cpp',
            'src/log/logfilter.cpp',
            'src/log/logtag.cpp',
            'src/log/logmetadata.cpp',
            'src/log/logwriters/journald.cpp',
            'src/log/logwriters/streamwriter.cpp',
            'src/log/logwriters/syslog.cpp',
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: include_dirs,
)

configmgr_lib = static_library(
        'configmgr',
        [
            'src/configmgr/overrides.cpp',
            'src/configmgr/configmgr-events.cpp',
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: include_dirs,
)

netcfgmgr_lib = static_library(
        'netcfgmgr',
        [
            'src/netcfg/proxy-netcfg-device.cpp',
            'src/netcfg/proxy-netcfg-mgr.cpp',
            'src/netcfg/netcfg-changeevent.cpp',
            'src/netcfg/netcfg-changetype.cpp',
            'src/netcfg/netcfg-signals.cpp',
            'src/netcfg/netcfg-subscriptions.cpp',
            'src/netcfg/dns/proxy-systemd-resolved.cpp',
            'src/netcfg/dns/systemd-resolved.cpp',
            'src/netcfg/dns/systemd-resolved-ipaddr.cpp',
            'src/netcfg/dns/resolvconf-file.cpp',
            'src/netcfg/dns/resolver-settings.cpp',
            'src/netcfg/dns/settings-manager.cpp',
        ],
        dependencies: [
            base_dependencies,
            dco_dependencies,
        ],
        include_directories: include_dirs,
)

sessionmgr_lib = static_library(
        'sessionmgr',
        [
            'src/sessionmgr/sessionmgr-events.cpp'
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: include_dirs,
)


#
#  Base D-Bus policy
#
#  This policy is generic across all services; each service has
#  their own policy on top of this one
configure_file(
    input: 'src/policy/net.openvpn.v3.conf.in',
    output: 'net.openvpn.v3.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)


#
# Generic documentation
#
install_data(
    'README.md',
    'QUICK-START.md',
    'COPYRIGHT.md',
    install_dir: docdir,
)

install_subdir(
    'docs/dbus',
    install_dir: docdir
)

# Build Doxygen documentation?
if (get_option('doxygen').enabled())
        subdir('doxygen')
endif

#
#  OpenVPN 3 Linux services
#
subdir('docs/man')
subdir('src/dbus/signals')
subdir('src/client')
subdir('src/log')
subdir('src/netcfg')
subdir('src/python')
subdir('src/sessionmgr')
subdir('src/configmgr')

subdir('src/ovpn3cli')
subdir('src/dbus')
subdir('src/selinux')
subdir('src/policy/polkit')

if dep_systemd.found()
    subdir('distro/systemd')
endif

if get_option('bash-completion').enabled()
   subdir('src/shell/bash-completion')
endif

if get_option('addon-aws').enabled()
   subdir('addons/aws')
endif

if get_option('addon-deviceposture').enabled()
   subdir('addons/devposture')
endif

#
#  Test programs
#
subdir('src/tests')
