#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>


# openvpn3-service-client

libcore_epoch = static_library(
    'core-epoch',
    [
        '../../openvpn3-core/openvpn/crypto/data_epoch.cpp',
    ],
    include_directories: [include_dirs, '../..'],
    cpp_args: ['-DUSE_OPENSSL', '-DUSE_ASIO'],
)

bin_backend_client = executable(
    'openvpn3-service-client',
    [
        'openvpn3-service-client.cpp',
        dco_keyconfig_cc,
        dco_keyconfig_h,
    ],
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
        dco_dependencies,
    ],
    link_with: [
        common_code,
        signals_code,
        configmgr_lib,
        netcfgmgr_lib,
        libcore_epoch,
    ],
    install: true,
    install_dir: get_option('libexecdir') / meson.project_name()
)
# D-Bus policy
configure_file(
    input: '../policy/net.openvpn.v3.client.conf.in',
    output: 'net.openvpn.v3.client.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)


#  openvpn3-service-backendstart
bin_backend_start = executable(
    'openvpn3-service-backendstart',
    [
        'openvpn3-service-backendstart.cpp',
    ],
    cpp_args: '-DBACKEND_CLIENT_BIN="' + bin_backend_client.name() + '"',
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        signals_code,
    ],
    install: true,
    install_dir: libexec_dir
)
# D-Bus policy
configure_file(
    input: '../policy/net.openvpn.v3.backends.conf.in',
    output: 'net.openvpn.v3.backends.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)
# D-Bus service autostart config
configure_file(
    input: '../service-autostart/template.service.in',
    output: 'net.openvpn.v3.backends.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.backends',
            'SERVICE_BIN': bin_backend_start.name(),
            'SERVICE_ARGS': '',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)
