#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

signals_code = static_library(
        'signals',
        [
            'attention-required.cpp',
            'log.cpp',
            'statuschange.cpp',
        ],
        dependencies: [
            base_dependencies,
        ],
        include_directories: include_dirs,
)
