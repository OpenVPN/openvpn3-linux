=======================
openvpn3-session-manage
=======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 session-manage`` ``[OPTIONS]``
| ``openvpn3 session-manage`` ``-h`` | ``--help``


DESCRIPTION
===========
This command manages running VPN sessions.

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

--log-level[=LEVEL]
                View/change the log-level for this session.  This will have an
                instant impact on the log level next time there are new log
                events from the VPN session.  If no new log level is given, only
                the current log level will be shown.

--timeout SECS
                Defines how long it will attempt to reestablish the connection
                when restarting or resuming a session.  Default is to never
                time out.

-P, --pause
                Suspends/pauses the running VPN session.  This is not a
                complete shutdown and can be used to temporarily disconnect with
                the intention to later re-establish the VPN tunnel.  This will
                in most cases not require re-entering any credentials.

-R, --resume
                Resumes a suspended/paused VPN session.

--restart
                This will trigger a complete reconnect of the VPN session.

-D, --disconnect
                This is a final connection disconnect.  This will remove the
                session in the session manager.  This can also be used to
                remove stray VPN sessions which did not complete successfully
                by itself.  Once disconnected, if connection statistics is
                available it will be printed to the terminal.

--cleanup
                Remove all stale sessions which have no VPN client backend
                process running.  These sessions typically have no status
                available.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-session-acl``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-stats``\(1)
``openvpn3-sessions-list``\(1)
