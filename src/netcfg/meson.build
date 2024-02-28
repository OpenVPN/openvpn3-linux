#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

bin_backend_netcfg = executable(
    'openvpn3-service-netcfg-dev',
    [
        'openvpn3-service-netcfg.cpp',
        'core-tunbuilder.cpp',
        'netcfg-dco.cpp',
        'netcfg-device.cpp',
        'netcfg-service.cpp',
        'netcfg-service-handler.cpp',
        'dns/systemd-resolved.cpp',
        'dns/resolvconf-file.cpp',
        'dns/resolver-settings.cpp',
        'dns/settings-manager.cpp',
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
        netcfgmgr_lib,
    ],
    install: true,
    install_dir: get_option('libexecdir') / meson.project_name()
)

# D-Bus policy
configure_file(
    input: '../policy/net.openvpn.v3.netcfg.conf.in',
    output: 'net.openvpn.v3.netcfg.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

# D-Bus service autostart config
configure_file(
    input: '../service-autostart/template.service.in',
    output: 'net.openvpn.v3.netcfg.service',
    configuration: configuration_data(
        dbus_config +
        {
            'BUSNAME': 'net.openvpn.v3.netcfg',
            'SERVICE_BIN': bin_backend_netcfg.name(),
            'SERVICE_ARGS': '--state-dir "' + openvpn3_statedir + '"',
            'OPENVPN_USERNAME': 'root',
        }
    ),
    install: true,
    install_dir: dbus_service_dir,
)
