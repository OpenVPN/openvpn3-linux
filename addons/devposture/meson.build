#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  Razvan Cojocaru <razvan.cojocaru@openvpn.com>

sysinfo_lib = static_library(
        'sysinfo',
        [
            'sysinfo/sysinfo.cpp',
        ],
        include_directories: include_dirs,
)

#  openvpn3-service-devposture
bin_devposture_start = executable(
    'openvpn3-service-devposture',
    [
        'openvpn3-service-devposture.cpp',
        'service.cpp',
        'modulehandler.cpp',
        'devposture-signals.cpp',
        'modules/platform.cpp',
        'modules/datetime.cpp',
    ],
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
      sysinfo_lib,
      common_code,
    ],
    install: true,
    install_dir: libexec_dir
)


# Sample protocol profile
install_data(
    'profiles/example1.json',
    'profiles/example2.json',
    'profiles/openvpninc.json',
    install_dir: devposture_profiledir
)

# D-Bus policy
configure_file(
    input: 'policy/net.openvpn.v3.devposture.conf.in',
    output: 'net.openvpn.v3.devposture.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

# D-Bus service autostart config
configure_file(
    input: '../../src/service-autostart/template.service.in',
    output: 'net.openvpn.v3.devposture.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.devposture',
            'SERVICE_BIN': bin_devposture_start.name(),
            'SERVICE_ARGS': '--profile-dir "' + devposture_profiledir + '"',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)

#
#  Test programs
#
subdir('tests')
