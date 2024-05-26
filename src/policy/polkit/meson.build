#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

polkit_rulesdir = get_option('polkit_rulesdir')
if polkit_rulesdir == ''
   polkit_rulesdir = get_option('datadir') / 'polkit-1/rules.d'
endif

configure_file(
    input: 'net.openvpn.v3.rules.in',
    output: 'net.openvpn.v3.rules',
    configuration: configuration_data(dbus_config),
    install: true,
    install_dir: polkit_rulesdir,
)

if get_option('use-legacy-polkit-pkla').enabled()
   polkit_pkladir = get_option('polkit_pkla_rulesdir')
   if polkit_pkladir == ''
       polkit_pkladir = get_option('localstatedir') / 'lib/polkit-1/localauthority/10-vendor.d/'
   endif

   configure_file(
       input: 'openvpn3-linux.pkla.in',
       output: 'openvpn3-linux.pkla',
       configuration: configuration_data(dbus_config),
       install: true,
       install_dir: polkit_pkladir,
   )
endif
