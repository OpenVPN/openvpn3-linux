#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024 -  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024 -  David Sommerseth <davids@openvpn.net>


configure_file(
    input: 'pyproject.toml.in',
    output: 'pyproject.toml',
    configuration: configuration_data(
        {
            'VERSION': meson.project_version(),
        }
    ),
)

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

python.install_sources(
        'openvpn3/__init__.py',
        'openvpn3/ConfigManager.py',
        'openvpn3/ConfigParser.py',
        'openvpn3/NetCfgManager.py',
        'openvpn3/SessionManager.py',
        subdir: 'openvpn3',
)

custom_target('openvpn2',
        input: 'openvpn2',
        output: 'openvpn2',
        command: [cp_prog, '@INPUT@', '@OUTPUT@'],
        install: true,
        install_dir: get_option('prefix') / 'bin',
)

custom_target('openvpn3-as',
        input: 'openvpn3-as',
        output: 'openvpn3-as',
        command: [cp_prog, '@INPUT@', '@OUTPUT@'],
        install: true,
        install_dir: get_option('prefix') / 'bin',
)

custom_target('openvpn3-autoload',
        input: 'openvpn3-autoload',
        output: 'openvpn3-autoload',
        command: [cp_prog, '@INPUT@', '@OUTPUT@'],
        install: true,
        install_dir: get_option('prefix') / 'sbin',
)
