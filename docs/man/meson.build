#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

#
# Generic man page setup
#
generate_man = not get_option('generate-man').disabled()
mandir_1 = get_option('mandir') / 'man1'
mandir_7 = get_option('mandir') / 'man7'
mandir_8 = get_option('mandir') / 'man8'
prog_rst2man = disabler()
if generate_man
    prog_rst2man = find_program('rst2man', 'rst2man.py',
                                required : get_option('generate-man').enabled(),
                                disabler: true)
endif
man_config = configuration_data({
    'OPENVPN_USERNAME': get_option('openvpn_username'),
})
