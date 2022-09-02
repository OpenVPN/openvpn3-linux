=====================
openvpn3-session-auth
=====================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 session-auth`` ``[OPTIONS]``
| ``openvpn3 session-auth`` ``-h`` | ``--help``


DESCRIPTION
===========
This command allows a user to see all on-going authentication events and pick
up stalled ones to complete them.  When running this command without any
arguments unresolved authentications are listed.

OPTIONS
=======

-h, --help
        Print  usage and help details to the terminal

--auth-req ID
        Continue the authentication process for the given auth request ID.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-sessions-list``\(1)
