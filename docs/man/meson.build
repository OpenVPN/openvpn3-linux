#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>


mandir_1 = get_option('mandir') / 'man1'
mandir_7 = get_option('mandir') / 'man7'
mandir_8 = get_option('mandir') / 'man8'

manpage_sources = [
    ['openvpn2.1.rst', mandir_1],
    ['openvpn3-as.1.rst', mandir_1],
    ['openvpn3-config-acl.1.rst', mandir_1],
    ['openvpn3-config-dump.1.rst', mandir_1],
    ['openvpn3-config-import.1.rst', mandir_1],
    ['openvpn3-config-manage.1.rst', mandir_1],
    ['openvpn3-config-remove.1.rst', mandir_1],
    ['openvpn3-configs-list.1.rst', mandir_1],
    ['openvpn3-log.1.rst', mandir_1],
    ['openvpn3-session-acl.1.rst', mandir_1],
    ['openvpn3-session-auth.1.rst', mandir_1],
    ['openvpn3-session-manage.1.rst', mandir_1],
    ['openvpn3-session-start.1.rst', mandir_1],
    ['openvpn3-session-stats.1.rst', mandir_1],
    ['openvpn3-sessions-list.1.rst', mandir_1],
    ['openvpn3.1.rst', mandir_1],

    ['openvpn3-linux.7.rst.in', mandir_7],

    ['openvpn3-admin-init-config.8.rst.in', mandir_8],
    ['openvpn3-admin-journal.8.rst', mandir_8],
    ['openvpn3-admin-log-service.8.rst.in', mandir_8],
    ['openvpn3-admin-netcfg-service.8.rst', mandir_8],
    ['openvpn3-admin-sessionmgr-service.8.rst', mandir_8],
    ['openvpn3-admin.8.rst', mandir_8],
    ['openvpn3-autoload.8.rst', mandir_8],
    ['openvpn3-service-backendstart.8.rst.in', mandir_8],
    ['openvpn3-service-client.8.rst.in', mandir_8],
    ['openvpn3-service-configmgr.8.rst.in', mandir_8],
    ['openvpn3-service-log.8.rst', mandir_8],
    ['openvpn3-service-netcfg.8.rst.in', mandir_8],
    ['openvpn3-service-sessionmgr.8.rst.in', mandir_8],
    ['openvpn3-systemd.8.rst', mandir_8],
]

#
# Generic man page setup
#
generate_man = not get_option('generate-man').disabled()
prog_rst2man = disabler()
if generate_man
    prog_rst2man = find_program('rst2man', 'rst2man.py',
                                required : get_option('generate-man').enabled(),
                                disabler: true)
endif
man_config = configuration_data({
    'OPENVPN_USERNAME': get_option('openvpn_username'),
    'OPENVPN_STATEDIR': openvpn3_statedir,
    'DBUS_SYSTEM_POLICYDIR': dbus_policy_dir,
    'DBUS_SYSTEM_SERVICEDIR': dbus_service_dir,
})


foreach manp : manpage_sources
    man_src = manp[0]
    man_dest = manp[1]
    if man_src.endswith('.rst.in')
        manpage_cfg = configure_file(
            input: man_src,
            output: man_src.replace('.rst.in', '.rst'),
            configuration: man_config,
        )
        custom_target(
            'man-' + man_src.replace('.rst.in', ''),
            input: manpage_cfg,
            output: man_src.replace('.rst.in', ''),
            command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
            install: true,
            install_dir: man_dest,
        )
    endif
    if man_src.endswith('.rst')
        custom_target(
            'man-' + man_src.replace('.rst', ''),
            input: man_src,
            output: man_src.replace('.rst', ''),
            command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
            install: true,
            install_dir: man_dest,
        )
    endif
endforeach
