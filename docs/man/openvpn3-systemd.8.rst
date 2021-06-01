================
openvpn3-systemd
================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-systemd`` ``[OPTIONS] CONFIG_PROFILE``
| ``openvpn3-systemd`` ``-h`` | ``--help``


DESCRIPTION
===========
This is a helper script used by the ``openvpn3-session@.service`` unit file,
which is responsible for running a single VPN session and retrieve log events
and related signals to the session.  These signals are also used to update
systemd with the appropriate status.

Using the ``openvpn3-session@.service`` requires the VPN configuration profile
to be imported in advance, using ``openvpn3 config-import``, preferably as a
persistent configuration to allow starting VPN sessions during boot.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--start         Start a new VPN session with the given configuration profile
                name.

--restart       Restarts a currently running VPN session with the given
                configuration profile name.

--stop          Stops a currently running VPN session with the given
                configuration profile name.

--log-level LEVEL
                Sets the log verbosity for the log events.  Valid values
                are :code:`0` to :code:`6`.  The higher value, the more
                verbose the log events will be.  Log level :code:`6` will
                include all debug events.  Default is :code:`5`.


EXAMPLE
=======
First a configuration profile is imported as a persistent profile:
::

   # openvpn3 config-import --persistent --name example --config client.ovpn

Start a VPN session via ``systemctl``\(1):
::

   # systemctl start openvpn3-session@example

Check the logs for this session:
::

   # journalctl --since today --unit openvpn3-session@example


KNOWN ISSUES
============
Currently the ``openvpn3-systemd`` helper does not support configuration
profiles requiring any type of user authentication outside of X.509
certificates.

SEE ALSO
========

``openvpn3-config-import``\(1)
``openvpn3-config-acl``\(1)
``openvpn3-linux``\(7)
