#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024 -  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024 -  David Sommerseth <davids@openvpn.net>


gen_py_consts = executable('gen-py-constants',
        [
                'openvpn3/gen-python-constants.cpp',
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: [include_dirs, '../..'],
)

py_consts = custom_target(
        'python-constants',
        output: 'constants.py',
        command: gen_py_consts,
        capture: true,
        install: true,
        install_dir: python.get_install_dir(pure: true) / 'openvpn3',
)
