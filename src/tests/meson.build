#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
#  Test programs
#

executable('request-queue-service',
    [
        'dbus/request-queue-service.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('request-queue-client1',
    [
        'dbus/request-queue-client1.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('request-queue-client2',
    [
        'dbus/request-queue-client2.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)
