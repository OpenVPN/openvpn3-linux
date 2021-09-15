=================================
openvpn3-admin-sessionmgr-service
=================================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-admin sessionmgr-service`` ``[OPTIONS]``
| ``openvpn3-admin sessionmgr-service`` ``-h`` | ``--help``


DESCRIPTION
===========
This is the management interface for the openvpn3-service-sessionmgr
service, with the net.openvpn.v3.sessions D-Bus service name.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--list-sessions
                Lists an overview over all running OpenVPN 3 VPN client sessions

--list-sessions-verbose
                Similar to `--list-sessions` but provides more details
                for each session.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-service-sessionmgr``\(8)
