#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>


gen_py_consts = executable('gen-py-constants',
        [
                'gen-python-constants.cpp',
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: [include_dirs, '../../..'],
)

py_consts = custom_target(
        'python-constants',
        output: 'constants.py',
        command: gen_py_consts,
        capture: true,
        install: true,
        install_dir: python.get_install_dir(pure: true) / 'openvpn3',
)

python.install_sources(
        '__init__.py',
        'ConfigManager.py',
        'ConfigParser.py',
        'NetCfgManager.py',
        'SessionManager.py',
        subdir: 'openvpn3',
)
