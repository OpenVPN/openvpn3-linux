#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
# Generic man page setup
#
generate_man = not get_option('generate-man').disabled()
mandir_1 = get_option('mandir') / 'man1'
mandir_7 = get_option('mandir') / 'man7'
mandir_8 = get_option('mandir') / 'man8'
prog_rst2man = disabler()
if generate_man
    prog_rst2man = find_program('rst2man', 'rst2man.py',
                                required : get_option('generate-man').enabled(),
                                disabler: true)
endif
man_config = configuration_data({
    'OPENVPN_USERNAME': get_option('openvpn_username'),
    'OPENVPN_STATEDIR': openvpn3_statedir,
})


#
#  man pages - src/client
#
man_backendstart = configure_file(
    input: 'openvpn3-service-backendstart.8.rst.in',
    output: 'openvpn3-service-backendstart.8.rst',
    configuration: man_config,
)
custom_target(
    'man-openvpn3-service-backendstart',
    input: man_backendstart,
    output: 'openvpn3-service-backendstart.8',
    command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: mandir_8,
)

man_backendclient = configure_file(
    input: 'openvpn3-service-client.8.rst.in',
    output: 'openvpn3-service-client.8.rst',
    configuration: man_config,
)
custom_target(
    'man-openvpn3-service-client',
    input: man_backendclient,
    output: 'openvpn3-service-client.8',
    command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: mandir_8,
)


#
#  man pages - src/log
#
custom_target(
    'man-openvpn3-service-log',
    input: 'openvpn3-service-log.8.rst',
    output: 'openvpn3-service-log.8',
    command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: mandir_8,
)


#
#  man pages - src/netcfg
#
man_netcfg = configure_file(
    input: 'openvpn3-service-netcfg.8.rst.in',
    output: 'openvpn3-service-netcfg.8.rst',
    configuration: man_config,
)
custom_target(
    'man-openvpn3-service-netcfg',
    input: man_netcfg,
    output: 'openvpn3-service-netcfg.8',
    command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: mandir_8,
)

#
#  man pages - src/sessionmgr
#
man_sessionmgr = configure_file(
    input: 'openvpn3-service-sessionmgr.8.rst.in',
    output: 'openvpn3-service-sessionmgr.8.rst',
    configuration: man_config,
)
custom_target(
    'man-openvpn3-service-sessionmgr',
    input: man_sessionmgr,
    output: 'openvpn3-service-sessionmgr.8',
    command: [prog_rst2man, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: mandir_8,
)
