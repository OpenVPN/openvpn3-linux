#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

cfgdata = configuration_data()
cfgdata.set('doxygen_builddir', meson.current_build_dir())
cfgdata.set('top_srcdir', meson.project_source_root())

doxygen_conf = configure_file(
    input: 'openvpn3-linux.doxy.in',
    output: 'openvpn3-linux.doxy',
    configuration: cfgdata,
)

logo = custom_target('doxygen-img',
    input: 'openvpn-logo.png',
    output: 'openvpn-logo.png',
    command: [cp_prog, '@INPUT@', '@OUTPUT@'],
)

mainpage = custom_target('doxygen-mainpage',
    input: '../README.md',
    output: 'mainpage.md',
    command: [cp_prog, '@INPUT@', '@OUTPUT@'],
)

run_target('doxygen',
    command : ['doxygen', doxygen_conf],
    depends: [logo, mainpage],
)
