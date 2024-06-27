#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>
#  Copyright (C)  Razvan Cojocaru <razvan.cojocaru@openvpn.com>

#
#  Test programs
#

executable('sysinfo',
    [
        'sysinfo/main.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
      sysinfo_lib
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../sysinfo'],
)

executable('devposture-proxy',
    [
        'devposture-proxy/main.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [
        include_dirs,
        '..',
        '../../../',
    ],
)
