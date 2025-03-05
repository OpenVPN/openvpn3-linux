#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024 -  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024 -  David Sommerseth <davids@openvpn.net>


gen_constants_bin = executable('gen-c-constants',
    [
            'gen-c-constants.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

custom_target(
    'gen-c-constants-h',
    output: 'constants.h',
    command: gen_constants_bin,
    capture: true,
    install: true,
    install_dir: get_option('includedir') / 'openvpn3'
)
