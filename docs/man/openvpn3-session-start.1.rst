======================
openvpn3-session-start
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 session-start`` ``[OPTIONS]``
| ``openvpn3 session-start`` ``-h`` | ``--help``


DESCRIPTION
===========
This starts a new VPN session, which means a VPN client process is started,
it gathers needed credentials, connects to the remote server and configures
a VPN interface with the VPN IP address, network routes and possibly DNS
settings on this host.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

-c CONFIG-FILE, --config CONFIG-FILE
                Name of the configuration profile to start.  The
                ``session-start`` command will first look for matching
                configuration profile names pre-loaded into the configuration
                manager.  If not found, it will attempt to load and parse a
                configuration file on the file system.

-p CONFIG-DBUS-PATH, --config-path CONFIG-DBUS-PATH
                Can be used instead of ``--config`` where the D-Bus path to
                the configuration profile in the configuration manager is given
                instead.  Use ``openvpn3 configs-list`` to retrieve a list of
                available configuration paths.

--persist-tun
                Can be used together with ``--config`` to ensure the connection
                will not tear-down the virtual VPN interface when reconnecting.
                See ``openvpn3-config-manage`` for details.

--timeout SECS
                Defines how long it will attempt to establish a connection
                before aborting.  Default is to never time out.

--background
                Starts the connection in the background after basic user
                credentials has been provided.  Using this option will require
                to use ``openvpn3 session-manage``\(1) for further interaction
                with the session.

--dco BOOL
                Enable kernel based Data Channel Offload for this session only.
                This moves the tunnelled network traffic to be handled inside
                the kernel.  This improves the processing of the network traffic
                and moves the encryption, decryption and packet authentication
                for the tunnelled network traffic to be handled inside the
                kernel instead of begin passed via the OpenVPN client process in
                user space.

                This option is only available if openvpn3-linux has been built
                with this support.

                *WARNING:*
                    This is currently a **tech preview** feature and is **not**
                    ready for production environments.  It also requires the
                    `ovpn-dco` kernel module to be installed to work.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-configs-list``\(1)
``openvpn3-session-acl``\(1)
``openvpn3-session-manage``\(1)
``openvpn3-session-stats``\(1)
``openvpn3-sessions-list``\(1)
