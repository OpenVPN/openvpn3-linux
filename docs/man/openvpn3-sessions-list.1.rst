======================
openvpn3-sessions-list
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 sessions-list`` ``-h`` | ``--help``


DESCRIPTION
===========
Lists all running VPN sessions, including configuration names and D-Bus
session paths.  It will only enlist sessions which the user has been granted
access to or owns.

The configuration name being returned is the name which was current when the
session was started.  If the configuration was renamed afterwards, both the
new and old name will be shown.  If the configuration profile has been removed
since the VPN session started, this will be indicated as well.

OPTIONS
=======

-h, --help               Print  usage and help details to the terminal

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-session-acl``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-start``\(1)
``openvpn3-session-stats``\(1)
