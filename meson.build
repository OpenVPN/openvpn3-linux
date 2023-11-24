#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

project(
        'openvpn3-linux','cpp',
        default_options : [ 'cpp_std=c++17', 'cpp_args=-Werror' ],
        license: 'AGPL-3.0-only',
        version: run_command(['scripts/get-version','--gui-version'], check: true).stdout().strip(),
        meson_version: '>= 0.58'
)

#  Dependencies
dep_lz4 = dependency('liblz4', required: false)
dep_selinux = dependency('libselinux', required: false)
dep_systemd = dependency('libsystemd', version: '>218', required: false)
dep_tinyxml2 = dependency('tinyxml2', required: false)

# This is not a link dependency for this project, but we need it
# to get the proper paths for policy and service config installation
dep_dbus = dependency('dbus-1')

base_dependencies = [
    dep_lz4,
    dep_selinux,
    dep_systemd,
    dep_tinyxml2,
    dependency('gdbuspp'),
    dependency('glib-2.0'),
    dependency('gio-2.0'),
    dependency('gio-unix-2.0'),
    dependency('libcap-ng'),
    dependency('jsoncpp'),
    dependency('openssl', version: '>= 1.0.2'),
    dependency('threads'),
    dependency('tinyxml2'),
    dependency('uuid'),
]

dco_dependencies = []
if get_option('dco').enabled()
    dco_dependencies = [
        dependency('libnl-genl-3.0', version: '>= 3.2.29'),
        dependency('protobuf', version: '>= 2.4.0'),
    ]
endif


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
if (dep_systemd.found())
    sd_id128 = compiler.has_function('sd_id128_to_string',
                                     prefix: '#include <systemd/sd-id128.h>',
                                     dependencies: dep_systemd)
endif

#
#  Setup additional include header dirs
#
asio_inc = get_option('asio_path') / 'asio' / 'include'
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

if get_option('dco').enabled()
    add_project_arguments('-DENABLE_OVPNDCO', language: 'cpp')
endif

#
# Various other settings
#
openvpn3_statedir = get_option('openvpn3_statedir')
if openvpn3_statedir == ''
    openvpn3_statedir = get_option('sharedstatedir') / 'openvpn3'
endif

#
#  Create a build configuration header file
#
libexec_dir = get_option('libexecdir') / meson.project_name()

build_config = configuration_data({
    'OPENVPN_DEBUG': get_option('debug_options') or get_option('debug'),
    'DEBUG_EXCEPTIONS': get_option('debug_exceptions') or get_option('debug'),
    'DEBUG_CORE_EVENTS': get_option('debug_core_events'),
    'DEBUG_RTNL': get_option('debug_netcfg_netlink'),
    'DEBUG_DISABLE_SECURITY_CHECKS': get_option('debug_disable_security_checks'),
    'HAVE_LZ4': dep_lz4.found(),
    'HAVE_SELINUX': dep_selinux.found(),
    'HAVE_SYSTEMD': sd_id128,
    'OVPN_TINYXML2_HAS_ERROR_NAME': dep_tinyxml2.version() >= '4.0.0',
    'OVPN_TINYXML2_HAS_ERROR_STR': dep_tinyxml2.version() >= '6.0.0',
    'ASIO_STANDALONE': true,
    'ASIO_NO_DEPRECATED': true,
    'USE_ASIO': true,
    'USE_OPENSSL': true,
    'USE_TUN_BUILDER': true,
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

# Prepare some version details
fs = import('fs')
if fs.exists(meson.project_source_root() / '.git')
    version_header = configuration_data()
    version_header.set_quoted('OPENVPN_VERSION',
                              run_command(openvpn3_core_inc / 'scripts/version', check: true).stdout().strip(),
                              description: 'OpenVPN 3 Core Library version')
    version_header.set_quoted('PACKAGE_GUIVERSION', meson.project_version(),
                              description: 'OpenVPN 3 Linux version identifier')
    version_header.set_quoted('PACKAGE_NAME', meson.project_name(),
                              description: 'Name of this project')
    build_version_h = configure_file(
        configuration: version_header,
        output: 'build-version.h'
    )
else
    build_version_h = 'src/common/build-version.h'
endif
meson.add_dist_script(find_program('scripts/prepare-build-version-h'))

# Unless test_programs are not explicitly enabled,
# only build them in debug builds
debug = get_option('debug')
build_test_programs = get_option('test_programs').enabled()
if (get_option('test_programs').auto())
    build_test_programs = get_option('debug')
endif

message('OpenVPN 3 Linux service binary directory: ' + get_option('prefix') / libexec_dir)

#
#  D-Bus configuration
dbus_policy_dir = dep_dbus.get_variable('datadir') / 'dbus-1' / 'system.d'
dbus_service_dir = dep_dbus.get_variable('system_bus_services_dir')
dbus_config = {
    'OPENVPN_USERNAME': get_option('openvpn_username'),
    'LIBEXEC_PATH': get_option('prefix') / libexec_dir,
}
message('D-Bus system bus services dir: ' + dbus_service_dir)
message('D-Bus system policy dir: ' + dbus_policy_dir)

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
            'src/common/timestamp.cpp',
            'src/common/utils.cpp',
            'src/log/dbus-log.cpp',
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
#  Test programs
#
subdir('src/tests')
