#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  Lev Stipakov <lev@openvpn.net>

bin_addon_aws = executable(
    'openvpn3-service-aws',
    [
        'openvpn3-service-aws.cpp',
    ],
    include_directories: [include_dirs, '../..'],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        signals_code,
        netcfgmgr_lib,
    ],
    install: true,
    install_dir: get_option('libexecdir') / meson.project_name()
)

configure_file(
    input: 'policy/net.openvpn.v3.aws.conf.in',
    output: 'net.openvpn.v3.aws.conf',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: dbus_policy_dir,
)

systemd_service_cfg = dependency('systemd')

configure_file(
    input: 'systemd/openvpn3-aws.service.in',
    output: 'openvpn3-aws.service',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: systemd_service_cfg.get_variable('systemdsystemunitdir'),
)

install_data(
    [
        'certs/af_south_1.crt',
        'certs/ap_east_1.crt',
        'certs/default.crt',
        'certs/eu_south_1.crt',
        'certs/me_south_1.crt',
        'certs/us_gov.crt',
    ],
    install_mode: 'rw-r--r--',
    install_dir: get_option('sysconfdir') / 'openvpn3' / 'awscerts',
)
