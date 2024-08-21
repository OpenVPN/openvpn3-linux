#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
#  unit tests
#

googletest = subproject('gtest')
gtest_deps = googletest.get_variable('gtest_main_dep')
gtest_inc = googletest.get_variable('gtest_incdir')

unit_tests = executable(
           'unit-tests',
           [
                'attention-req.cpp',
                'configfileparser.cpp',
                'core-extensions.cpp',
                'dns-resolver-settings.cpp',
                'dns-settings-manager-test.cpp',
                'logevent.cpp',
                'logmetadata.cpp',
                'lookup.cpp',
                'machine-id.cpp',
                'netcfg-changeevent.cpp',
                'platforminfo.cpp',
                'sessionmgr-events.cpp',
                'statusevent.cpp',
                'syslog-facility-mapping.cpp',
                'systemd-resolved-ipaddr.cpp',
                'timestamp.cpp',
                '../../netcfg/dns/resolver-settings.cpp',
                '../../netcfg/dns/settings-manager.cpp',
           ],
           include_directories: [include_dirs, gtest_inc, '../../..'],
           link_with: [
                 common_code,
                 netcfgmgr_lib,
                 sessionmgr_lib,
           ],
           dependencies : [
                base_dependencies,
                dco_dependencies,
                gtest_deps,
           ],
          build_by_default: build_unit_tests,
)
test('unit-tests',
        unit_tests,
        protocol: 'gtest',
        suite: 'unit'
)
