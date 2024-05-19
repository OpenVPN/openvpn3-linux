#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

commands_collection = static_library(
    'commandscollection',
    [
        'arghelpers.cpp',
        'commands/init-config.cpp',
        'commands/journal.cpp',
        'commands/variables.cpp',
        'commands/version.cpp',
        'commands/version-services.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('openvpn3-dev',
    [
        'openvpn3.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        commands_collection,
    ],
    include_directories: [include_dirs, '../..'],
    install: false,
)

executable('openvpn3-admin-dev',
    [
        'openvpn3-admin.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        commands_collection,
        netcfgmgr_lib,
    ],
    include_directories: [include_dirs, '../..'],
    install: false,
)
