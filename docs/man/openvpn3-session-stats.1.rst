======================
openvpn3-session-stats
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 session-stats`` ``[OPTIONS]``
| ``openvpn3 session-stats`` ``-h`` | ``--help``


DESCRIPTION
===========
Provides up-to-date statistics for a specific VPN session.  The content in the
result is dynamic, as only the events registered will be displayed.  Typically
it will report bytes and packets sent, reconnection attempts, time-outs and
related events.

OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

-o SESSION-PATH, --path SESSION-PATH
                D-Bus session path to the currently running session to query

--session-path DBUS-PATH
                Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                Can be used instead of ``--path`` where the configuration
                profile name is given instead.  The *CONFIG_NAME* must be the
                configuration name which was active when the session was
                started.

-I INTERFACE, --interface INTERFACE
                Can be used instead of ``--path`` where the tun interface name
                managed by OpenVPN 3 is given instead.

-j, --json
                Format the output as JSON instead of formatted plain text.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-session-acl``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-start``\(1)
``openvpn3-session-stats``\(1)
