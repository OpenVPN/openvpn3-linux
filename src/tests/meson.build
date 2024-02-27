#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
#  Test programs
#

subdir('unit')

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

executable('fetch-avail-config-path',
    [
        'dbus/fetch-avail-config-paths.cpp'
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

executable('config-lock-down',
    [
        'dbus/config-lock-down.cpp'
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

executable('config-override-selftest',
    [
        'dbus/config-override-selftest.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('config-fetch',
    [
        'dbus/fetch-config.cpp'
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

executable('config-fetch2',
    [
        'dbus/fetch-config2.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('config-get-overrides',
    [
        'dbus/get-config-overrides.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('get-acl',
    [
        'dbus/get-acl.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

logevent_selftest = executable('logevent-selftest',
    [
        'dbus/logevent-selftest.cpp',
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
test('logevent-selftest',
    logevent_selftest)


executable('manager-lookupconfigname',
    [
        'dbus/manager-lookupconfigname.cpp'
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('platforminfo',
    [
        'misc/platforminfo.cpp'
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

executable('netcfg-systemd-resolved-basic',
    [
        'dbus/netcfg-systemd-resolved-basic.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        netcfgmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('netcfg-proxy-unit',
    [
        'dbus/netcfg-proxy-unit.cpp',
        dco_keyconfig_cc,
        dco_keyconfig_h,
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        netcfgmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('netcfg-changeevent-selftest',
    [
        'dbus/netcfg-changeevent-selftest.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        netcfgmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)
