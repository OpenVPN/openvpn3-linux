#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

selinux_devel_path = get_option('selinux_devel_path')
if (selinux_devel_path == '')
    selinux_devel_path = get_option('prefix') / get_option('datadir') / 'selinux/devel'
endif

build_selinux = get_option('selinux_policy').enabled()
cmd_make = find_program('make', required: build_selinux)
cmd_bzip2 = find_program('bzip2', required: build_selinux)
selinux_makefile = selinux_devel_path / 'Makefile'
if (not cmd_make.found() or not cmd_bzip2.found())
    build_selinux = false
else
    build_selinux = get_option('selinux_policy').enabled() or get_option('selinux_policy').auto()
    if (not fs.exists(selinux_makefile))
        warning('SELinux policy compilation Makefile missing: ' + selinux_makefile)
        build_selinux = false
    endif
endif
message('SELinux policy compilation: ' + build_selinux.to_string())
if (get_option('selinux_policy').enabled() and not build_selinux)
    error('Cannot continue with SELinux policy building enforced')
endif
if (build_selinux)
    message('SELinux policy development dir:' + selinux_devel_path)
endif

policy_builder = find_program(meson.project_source_root() / 'scripts/build-selinux-policy')
builder_args = [
        policy_builder,
        '@CURRENT_SOURCE_DIR@',
        '@PRIVATE_DIR@',
        selinux_makefile,
        '@OUTPUT@'
]

custom_target(
    'openvpn3.pp.bz2',
    input: ['openvpn3.te','openvpn3.fc','openvpn3.if'],
    output: 'openvpn3.pp.bz2',
    command: builder_args,
    build_by_default: build_selinux,
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'selinux/packages',
)

custom_target(
    'openvpn3_service.pp.bz2',
    input: ['openvpn3_service.te','openvpn3_service.fc', 'openvpn3_service.if'],
    output: 'openvpn3_service.pp.bz2',
    command: builder_args,
    build_by_default: build_selinux,
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'selinux/packages',
)
