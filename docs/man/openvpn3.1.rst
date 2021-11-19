========
openvpn3
========

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3`` [ COMMAND ] [ OPTIONS ]
| ``openvpn3`` [ COMMAND ] ``-h`` | ``--help``
| ``openvpn3`` ``-h`` | ``--help`` | ``help``


DESCRIPTION
===========
The **openvpn3** utility is the main management tool for managing and
configuring OpenVPN configuration profiles as well as VPN sessions.

This utility is based upon a "command" approach, where the first argument
to **openvpn3** will always be a command operator.  Each of the available
commands have their own set of options.


COMMANDS
========

``version``
    Show OpenVPN 3 version information

Configuration management
------------------------
``config-import``
    Import configuration profiles

``config-manage``
    Manage configuration properties

``config-acl``
    Manage access control lists for configurations

``config-dump``
    Show/dump a configuration profile

``config-remove``
    Remove an available configuration profile

``configs-list``
    List all available configuration profiles

Session management
------------------
``session-start``
    Start a new VPN session

``session-manage``
    Manage VPN sessions

``session-auth``
    See and interact with on-going session authentication

``session-acl``
    Manage access control lists for sessions

``session-stats``
    Show session statistics

``sessions-list``
    List available VPN sessions


Log commands
------------
``log``
    Receive log events as they occur

SEE ALSO
========

``openvpn2``\(1)
``openvpn3-config-import``\(1)
``openvpn3-config-acl``\(1)
``openvpn3-config-dump``\(1)
``openvpn3-config-remove``\(1)
``openvpn3-configs-list``\(1)
``openvpn3-session-start``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-auth``\(1)
``openvpn3-session-acl``\(1)
``openvpn3-session-stats``\(1)
``openvpn3-sessions-list``\(1)
``openvpn3-log``\(1)
``openvpn3-admin``\(8)

