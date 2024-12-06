#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

bin_backend_log = executable(
    'openvpn3-service-log',
    [
        'openvpn3-service-log.cpp',
        'log-service.cpp',
        'log-proxylog.cpp',
    ],
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        signals_code,
        sessionmgr_lib,
    ],
    install: true,
    install_dir: get_option('libexecdir') / meson.project_name()
)

# D-Bus policy
configure_file(
    input: '../policy/net.openvpn.v3.log.conf.in',
    output: 'net.openvpn.v3.log.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

# D-Bus service autostart config
configure_file(
    input: '../service-autostart/template.service.in',
    output: 'net.openvpn.v3.log.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.log',
            'SERVICE_BIN': bin_backend_log.name(),
            'SERVICE_ARGS': '--state-dir "' + openvpn3_statedir + '"',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)
