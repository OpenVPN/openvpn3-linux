#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

bashcompl_dir = get_option('datadir') / 'bash-completion/completions'

custom_target(
    'bash-completion-openvpn2',
    command: [
                find_program('gen-openvpn2-completion.py'),
                '--python-source-dir',
                meson.project_source_root() / 'src' / 'python' / 'openvpn3',
             ],
    output: 'openvpn2',
    capture: true,
    build_by_default: true,
    install: true,
    install_dir: get_option('prefix') / bashcompl_dir,
)

install_data(
    'openvpn3',
    install_dir: get_option('prefix') / bashcompl_dir,
)

# NOTE: Can be replaced with install_symlink() when Meson 0.61 or newer
#       is available on all supported distros
meson.add_install_script('sh','-c',
        'cd $MESON_INSTALL_DESTDIR_PREFIX/@0@; ln -sf openvpn3 openvpn3-admin'.format(bashcompl_dir))
