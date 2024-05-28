#  OpenVPN 3 Linux - Next generation OpenVPN
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2024-  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2024-  David Sommerseth <davids@openvpn.net>

commands_collection = static_library(
    'commandscollection',
    [
        'arghelpers.cpp',
        'commands/config/configs-list.cpp',
        'commands/config/config-acl.cpp',
        'commands/config/config-dump.cpp',
        'commands/config/config-import.cpp',
        'commands/config/config-manage.cpp',
        'commands/config/config-remove.cpp',
        'commands/session/helpers.cpp',
        'commands/session/sessions-list.cpp',
        'commands/session/session-acl.cpp',
        'commands/session/session-auth.cpp',
        'commands/session/session-manage.cpp',
        'commands/session/session-start.cpp',
        'commands/session/session-stats.cpp',
        'commands/init-config.cpp',
        'commands/journal.cpp',
        'commands/log/event-logger.cpp',
        'commands/log/log.cpp',
        'commands/log/log-attach.cpp',
        'commands/log-service.cpp',
        'commands/netcfg-service.cpp',
        'commands/sessionmgr-service.cpp',
        'commands/variables.cpp',
        'commands/version.cpp',
        'commands/version-services.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    include_directories: [include_dirs, '../..'],
)

executable('openvpn3',
    [
        'openvpn3.cpp',
        '../dbus/signals/log.cpp',
        '../dbus/signals/statuschange.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        commands_collection,
        common_code,
        configmgr_lib,
        sessionmgr_lib,
    ],
    include_directories: [include_dirs, '../..'],
    install: true,
)

executable('openvpn3-admin',
    [
        'openvpn3-admin.cpp',
    ],
    dependencies: [
        base_dependencies,
    ],
    link_with: [
        common_code,
        commands_collection,
        netcfgmgr_lib,
    ],
    include_directories: [include_dirs, '../..'],
    install: true,
    install_dir: get_option('prefix') / get_option('sbindir'),
)
