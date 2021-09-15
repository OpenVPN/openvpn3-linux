========
openvpn3
========

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-admin`` [ COMMAND ] [ OPTIONS ]
| ``openvpn3-admin`` [ COMMAND ] ``-h`` | ``--help``
| ``openvpn3-admin`` ``-h`` | ``--help`` | ``help``


DESCRIPTION
===========
The **openvpn3-admin** utility is a configuration tool for the OpenVPN 3
D-Bus services which supports management over D-Bus.

This utility is based upon a "command" approach, where the first argument
to **openvpn3-admin** will always be a command operator.  Each of the
available commands have their own set of options.


COMMANDS
========

version ``[--services]``
                Show program version information.  If ``--services`` is
                provided, it will show the version information for all available
                OpenVPN 3 D-Bus backend services.

variables ``[--machine-id]``
                Show various OpenVPN 3 Linux related variables.

                ``--machine-id`` shows the hosts machine-id and the source
                used to derive the value.  This value is sent to OpenVPN
                servers in the IV_HWADDR variable.

log-service
                Manage the OpenVPN 3 Log service

                * D-Bus service: *net.openvpn.v3.logger*
                * Provided by: **openvpn3-service-logger**\(8)

netcfg-service
                Manage the OpenVPN 3 Network Configuration service

                * D-Bus service: *net.openvpn.v3.netcfg*
                * Provided by: **openvpn3-service-netcfg**\(8)

sessionmgr-service
                Manage the OpenVPN 3 Session Manager service

                * D-Bus service: *net.openvpn.v3.sessions*
                * Provided by: **openvpn3-service-sessionmgr**\(8)

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-admin-log-service``\(8)
``openvpn3-admin-netcfg-service``\(8)
``openvpn3-admin-sessionmgr-service``\(8)
``openvpn3-service-logger``\(8)
``openvpn3-service-netcfg``\(8)
``openvpn3-service-sessionmgr``\(8)

