#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
#  Test programs
#

mkdir = find_program('mkdir')
test_workdir = meson.project_build_root() / 'test-workdir'
run_command([mkdir, test_workdir] )

if (build_unit_tests)
    subdir('unit')
endif

requeue_srv = executable('request-queue-service',
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

requeue_test = executable('request-queue-test',
    [
        'dbus/request-queue-test.cpp'
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
test('request-queue-test',
    requeue_test,
    args: [ requeue_srv, '100' ],
    timeout: 10,
    suite: 'dbus',
    workdir: test_workdir,
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

executable('cmdparser-test',
    [
        'command-parser/cmdparser.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        dep_jsoncpp,
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

cfg_override_sft = executable('config-override-selftest',
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
test('config-override-selftest',
    cfg_override_sft,
    suite: 'post-install')

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

executable('fetch-avail-session-paths',
    [
        'dbus/fetch-avail-session-paths.cpp',
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

executable('getconnectionstats',
    [
        'dbus/getconnectionstats.cpp',
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

executable('getlastlogevent',
    [
        'dbus/getlastlogevent.cpp',
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

executable('getlaststatus',
    [
        'dbus/getlaststatus.cpp',
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

executable('get-service-version-prop',
    [
        'dbus/get-service-version-prop.cpp',
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

executable('journal-log-parse',
    [
        'misc/journal-log-parse-test.cpp',
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
    logevent_selftest,
    suite: 'standalone')


executable('log-listener',
    [
        'dbus/log-listener.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        signals_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('logservice1',
    [
        'dbus/logservice1.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        signals_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

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

platinfo_test = executable('platforminfo',
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
test('platforminfo',
    platinfo_test,
    suite: 'dbus')

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

netcfg_chev_sft = executable('netcfg-changeevent-selftest',
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
test('netcfg-changeevent-selftest',
    netcfg_chev_sft,
    suite: 'standalone')

netcfg_dns_dir_sft = executable('netcfg-dns-direct-file-selftest',
    [
        'misc/netcfg-dns-direct-file-selftest.cpp',
        '../netcfg/dns/resolver-settings.cpp',
        '../netcfg/dns/resolvconf-file.cpp',
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
test('netcfg-dns-direct-file-selftest',
    netcfg_dns_dir_sft,
    suite: 'standalone',
    workdir: test_workdir
)

executable('signal-listener',
    [
        'dbus/signal-listener.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        netcfgmgr_lib,
        sessionmgr_lib,
        configmgr_lib,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('logfwd-listener',
    [
        'dbus/logfwd-listener.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        sessionmgr_lib,
        signals_code,
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('open-uri-test',
    [
        'misc/open-uri-test.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('ovpncli-netcfg',
    [
        'netcfg/cli.cpp',
        dco_keyconfig_cc,
        dco_keyconfig_h,
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
        netcfgmgr_lib,
        signals_code,

    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('profilemerge-optionlist',
    [
        'ovpn3-core/profilemerge-optionlist.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        dep_jsoncpp,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('config-export-json-test',
    [
        'misc/config-export-json-test.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        dep_jsoncpp,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('json-config-import-test',
    [
        'misc/json-config-import-test.cpp',
    ],
    build_by_default: build_test_programs,
    link_with: [
        common_code,
    ],
    dependencies: [
        dep_jsoncpp,
    ],
    include_directories: [include_dirs, '../..'],
)
