#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

bin_backend_configmgr = executable(
    'openvpn3-service-configmgr',
    [
        'openvpn3-service-configmgr.cpp',
        'configmgr-service.cpp',
        'configmgr-events.cpp',
        'configmgr-configuration.cpp',
        'configmgr-signals.cpp',
        'overrides.cpp',
    ],
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
    ],
    install: true,
    install_dir: get_option('libexecdir') / meson.project_name()
)

# D-Bus policy
configure_file(
    input: '../policy/net.openvpn.v3.configuration.conf.in',
    output: 'net.openvpn.v3.configuration.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

# D-Bus service autostart config
configure_file(
    input: '../service-autostart/template.service.in',
    output: 'net.openvpn.v3.configuration.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.configuration',
            'SERVICE_BIN': bin_backend_configmgr.name(),
            'SERVICE_ARGS': '--state-dir "' + openvpn3_statedir + '/configs"',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)

if get_option('create_statedir')
    # Create the configs directory for persistent configuration profiles
    # NOTE: Can be replaced with install_emptydir() when Meson 0.60 or newer
    #       is available on all supported distros
    meson.add_install_script('sh','-c', 'mkdir -p $DESTDIR@0@'.format(openvpn3_statedir / 'configs'))
endif
