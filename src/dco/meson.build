#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C)  David Sommerseth <davids@openvpn.net>

dco_dependencies = []
dco_keyconfig_h = []
dco_keyconfig_cc = []

if (dco_enabled)
    dco_dependencies = [
        dependency('libnl-genl-3.0', version: '>= 3.2.29'),
        dependency('protobuf', version: '>= 2.4.0'),
    ]
    protoc = find_program('protoc', required : true)
    cp_prog = find_program('cp', required: true)

    protogen = generator(
        protoc,
        output: ['@BASENAME@.pb.cc','@BASENAME@.pb.h'],
        arguments : [
            '--proto_path=@CURRENT_SOURCE_DIR@',
            '--cpp_out=@BUILD_DIR@', '@INPUT@'
        ],
    )
    dco_protobuf_gen = protogen.process('dco-keyconfig.proto')

    dco_keyconfig_cc = custom_target(
        'dco-keyconfig.pb.cc',
        command: [ cp_prog, '@INPUT0@', '@OUTPUT@'],
        input: dco_protobuf_gen,
        output: 'dco-keyconfig.pb.cc',
    )
    dco_keyconfig_h = custom_target(
        'dco-keyconfig.pb.h',
        command: [ cp_prog, '@INPUT1@', '@OUTPUT@'],
        input: dco_protobuf_gen,
        output: 'dco-keyconfig.pb.h',
    )
endif
