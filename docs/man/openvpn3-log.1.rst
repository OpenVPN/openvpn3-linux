============
openvpn3-log
============

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 log`` ``[OPTIONS]``
| ``openvpn3 log`` ``-h`` | ``--help``


DESCRIPTION
===========
This retrieves log events from various log senders and prints it to the
terminal.  The user running this command must in advanced have been granted
access to these log events, otherwise it will not generate any output.

Log events received via ``openvpn3 log`` are processed in parallel with the
system wide ``openvpn3-service-logger`` process.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--session-path SESSION-DBUS-PATH
                D-Bus session path to a running VPN session to retrieve log
                events from.  Use ``openvpn3 sessions-list`` to retrieve a list
                of available session D-Bus paths.

-c CONFIG-NAME, --config CONFIG-NAME
                Can be used instead of ``--session-path`` where the
                configuration profile name is given instead.  The *CONFIG_NAME*
                must be the name which was active when the session was started.
                Available configuration names can be found via
                ``openvpn3 sessions-list``.

-I INTERFACE, --interface INTERFACE
                Can be used instead of ``--session-path`` where the tun
                interface name managed by OpenVPN 3 is given instead.

--log-level LEVEL
                Sets the log verbosity for the log events.  Valid values
                are :code:`0` to :code:`6`.  The higher value, the more
                verbose the log events will be.  Log level :code:`6` will
                include all debug events.

--config-events
                Retrieve log events from the configuration manager.  For this
                to work, the ``openvpn3-service-configmgr`` must have been
                started with ``--signal-broadcast``.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-service-configmgr``\(8)
``openvpn3-service-logger``\(8)
``openvpn3-session-acl``\(1)
