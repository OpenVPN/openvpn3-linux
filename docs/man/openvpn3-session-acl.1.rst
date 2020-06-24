====================
openvpn3-session-acl
====================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 session-acl`` ``[OPTIONS]``
| ``openvpn3 session-acl`` ``-h`` | ``--help``


DESCRIPTION
===========
Each running VPN session has its own Access Control List associated with it.
This enables sessions to be managed by more users, where specific or all users
can be granted access to ``openvpn3 session-manage``, ``openvpn3 session-stats``
and ``openvpn3 log`` commands for a specific VPN session.

All options below can be used together.  If the ``--show`` option is used, it
will list the current Access Control List after any changes has been performed.

OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

-o SESSION-DBUS-PATH, --path SESSION-DBUS-PATH
                Required.  D-Bus session path to the currently running session
                to manage.  Use ``openvpn3 sessions-list`` to retrieve a list
                of available session D-Bus paths.

--session-path DBUS-PATH
                Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                Can be used instead of ``--path`` where the configuration
                profile name is given instead.  The *CONFIG_NAME* must be the
                configuration name which was active when the session was
                started.  Available configuration names can be found via
                ``openvpn3 sessions-list``.

-I INTERFACE, --interface INTERFACE
                Can be used instead of ``--path`` where the tun interface name
                managed by OpenVPN 3 is given instead.

-s, --show
                Shows the currently active ACL.

-G USER, --grant USER
                Grant the given user read-only access to this VPN session.
                The USER argument can be either UID or username belonging
                to the system.

-R USER, --revoke USER
                Revoke access on this VPN session for the given user.  The
                USER argument can be either UID or username belonging to
                the system.

--public-access BOOL
                Grant all users on the system access to manage this VPN session.
                This effectively disables the more fine-grained access control
                provided via ``--grant``.  Valid argument values: :code:`true`,
                :code:`false`.

--allow-log-access BOOL
                By default, users granted access will not have access to the
                session log.  By setting this option to *true*, users granted
                access can use ``openvpn3 log`` to retrieve real-time log events
                as they occur.  Valid argument values: :code:`true:`,
                :code:`false`.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-log``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-start``\(1)
``openvpn3-session-stats``\(1)
``openvpn3-sessions-list``\(1)
