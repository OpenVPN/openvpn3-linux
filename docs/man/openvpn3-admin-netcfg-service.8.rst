=============================
openvpn3-admin-netcfg-service
=============================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-admin netcfg-service`` ``[OPTIONS]``
| ``openvpn3-admin netcfg-service`` ``-h`` | ``--help``


DESCRIPTION
===========
This is the management interface for the openvpn3-service-netcfg
service, with the net.openvpn.v3.netcfg D-Bus service name.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--list-subscribers
                Lists all services which has been granted a subscription for
                NetworkChange signals and which change types they have
                requested a notification for.  These signals are sent each time
                a virtual network adapter is created, modified or destroyed,
                including DNS changes.

--unsubscribe DBUS-UNIQUE-NAME
                This can forcefully unsubscribe a service from receiving
                NetworkChange signals.  The argument must be the unique
                D-Bus name (typically **:1.xxxx** where **x** denotes an
                integer).

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-service-netcfg``\(8)
