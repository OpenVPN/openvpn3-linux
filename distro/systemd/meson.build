#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024 -  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024 -  David Sommerseth <davids@openvpn.net>


systemd_cfg = configuration_data({
    'LIBEXECDIR': get_option('prefix') / libexec_dir,
    'SYSCONFDIR': get_option('sysconfdir'),
    'SBINDIR': get_option('prefix') / 'sbin',
    }
)

systemd_service_cfg = dependency('systemd')

systemd_system_unit_dir = get_option('systemd_system_unit_dir')
if systemd_system_unit_dir == ''
  systemd_system_unit_dir = systemd_service_cfg.get_variable('systemdsystemunitdir')
endif

configure_file(
    input: 'openvpn3-autoload.service.in',
    output: 'openvpn3-autoload.service',
    configuration: systemd_cfg,
    install: true,
    install_dir: systemd_system_unit_dir,
)

configure_file(
    input: 'openvpn3-session@.service.in',
    output: 'openvpn3-session@.service',
    configuration: systemd_cfg,
    install: true,
    install_dir: systemd_system_unit_dir,
)

custom_target('openvpn3-systemd',
    input: 'openvpn3-systemd',
    output: 'openvpn3-systemd',
    command: [cp_prog, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: libexec_dir,
)
