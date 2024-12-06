#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

bin_backend_sessionmgr = executable(
    'openvpn3-service-sessionmgr',
    [
        'openvpn3-service-sessionmgr.cpp',
        'sessionmgr-service.cpp',
        'sessionmgr-signals.cpp',
        'sessionmgr-session.cpp',
        'tunnel-queue.cpp',
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
    input: '../policy/net.openvpn.v3.sessions.conf.in',
    output: 'net.openvpn.v3.sessions.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

# D-Bus service autostart config
configure_file(
    input: '../service-autostart/template.service.in',
    output: 'net.openvpn.v3.sessions.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.sessions',
            'SERVICE_BIN': bin_backend_sessionmgr.name(),
            'SERVICE_ARGS': '',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)
