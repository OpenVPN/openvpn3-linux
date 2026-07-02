#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2025-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2025-  David Sommerseth <davids@openvpn.net>


systemd_cfg = configuration_data({
    'BINDIR': get_option('prefix') / get_option('bindir')
})

if systemd_service_cfg.found()
    systemd_user_unit_dir = get_option('systemd_user_unit_dir')
    if systemd_user_unit_dir == ''
        systemd_user_unit_dir = systemd_service_cfg.get_variable('systemduserunitdir')
    endif

    configure_file(
        input: 'openvpn3-desktop-session-watcher.service.in',
        output: 'openvpn3-desktop-session-watcher.service',
        configuration: systemd_cfg,
        install: true,
        install_dir: systemd_user_unit_dir,
    )
endif

custom_target('openvpn3-desktop-session-watcher',
    input: 'openvpn3-desktop-session-watcher.py',
    output: 'openvpn3-desktop-session-watcher',
    command: [cp_prog, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_mode: 'rwxr-xr-x',
    install_dir: systemd_cfg.get('BINDIR'),
)
