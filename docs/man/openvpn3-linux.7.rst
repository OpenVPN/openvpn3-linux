==============
openvpn3-linux
==============

-------------------------------
OpenVPN 3 Linux client overview
-------------------------------

:Manual section: 7
:Manual group: OpenVPN 3 Linux


DESCRIPTION
===========
The OpenVPN 3 Linux client is a completely different VPN client compared to
the more classic OpenVPN 2.x generation.  Where OpenVPN 2.x is simplistic
in all kind of integration aspects, it carries several challenges due to the
security enhancements on more modern Linux distributions.

The OpenVPN 3 Linux client is a complete rewrite and makes use of the facilities
available on modern Linux distributions, which enables a safer execution model
with lesser security traps and obstacles caused by security modules on the
system.  This allows unprivileged users to start VPN sessions out-of-the-box,
which also takes care of the complete VPN configuration, including DNS setup.
In addition, OpenVPN 3 Linux will run with as few privileges as possible in
every aspect of the application layers.

To make this possible, OpenVPN 3 Linux is split up into several parts.


USER FRONT-ENDS
===============

There are several user front-ends tools which is used to interact with the
various components of OpenVPN 3 Linux.

* ``openvpn2``

  This aims to be fairly similar to the more classic ``openvpn``\(8)
  command line, with the exception that it only processes client related
  options.

* ``openvpn3``

  This is the main utility for managing all the features available in
  OpenVPN 3 Linux and has a different user interface than ``openvpn2``\(1)
  or ``openvpn``\(8).  On systems with the bash-completion integration
  installed, almost every command, option and argument used by
  ``openvpn3`` will get auto-completion help.

* ``openvpn3-admin``

  This utility is mostly targeting system administrators and provides
  interfaces to do some management of the various OpenVPN 3 D-Bus
  services.  See ``openvpn3-admin``\(8) for more details.

* ``openvpn3-autoload``

  This is not strictly a utility most users will use too often.  But it
  can be used to automatically load VPN configuration profiles as well as
  starting them during boot or login.

In addition, a Python 3 module is also available making it possible to write
your own OpenVPN 3 front-ends with little extra efforts.  Both ``openvpn2``
and ``openvpn3-autoload`` are Python 3 scripts which makes use of this
OpenVPN 3 Python module.

OpenVPN 3 Linux also provides an ``openvpn3-systemd``\(8) helper, which
allows starting and managing VPN sessions via ``systemd``\(1)


OPENVPN 3 D-BUS SERVICES
========================

The front-end interfaces talks to services which are started automatically
when needed and usually stops running if they are not in use.  There are
several services, where each service only is responsible for a fairly small
set of features.

* **Configuration Manager** (``openvpn3-service-configmgr``)

  This provides a service which only takes care of managing VPN
  configuration profiles for all users on the system.  It has a built in
  Access Control List mechanism, which ensures only users granted access
  can access the information they need.

  D-Bus name: *net.openvpn.v3.configuration*


* **Session Manager** (``openvpn3-service-sessionmgr``)

  This service takes care of managing running OpenVPN sessions on the
  system.  This has also its own set of Access Control List mechanisms,
  where users on the system can be granted access to VPN sessions
  started by other users.  The default behaviour is to only let the
  user starting a VPN session manage and access it.

  D-Bus name: *net.openvpn.v3.sessions*

* **Backend VPN Client** (``openvpn3-service-client``)

  This is the core VPN client.  There will be running one such process
  per active VPN session.

  D-Bus name: *net.openvpn.v3.backends.be${PID}*


* **Backend VPN Client Starter** (``openvpn3-service-backendstart``)

  This is a helper service which is used by the Session Manager to start
  new Backend VPN Client processes.

  D-Bus name: *net.openvpn.v3.backends*

* **Network Configuration** (``openvpn3-service-netcfg``)

  This is the most privileged D-Bus service, and is used by the
  Backend VPN Client processes.  This is responsible for creating and
  manage virtual network interfaces, configure IP addresses, routes in
  addition to handling the DNS setup.

  D-Bus name: *net.openvpn.v3.netcfg*

* **Logging** (``openvpn3-service-logger``)

  This is responsible to receive log events from all the services above
  and ensure they get logged properly.  Whether that is to a terminal, a
  log file or syslog, depends on the configuration.  The default is
  to start this service using the syslog integration.

  D-Bus name: *net.openvpn.v3.log*

All of these services above are D-Bus services which is available on the
system bus.  They are all automatically started when needed, with the bare
minimum of privileges they need.


D-BUS POLICY
============

D-Bus enforces usage of a policy which defines who can access which D-Bus
service and which features, objects and properties provided by a D-Bus service.
This is defined in ``/etc/dbus-1/systemd.d/net.openvpn.v3.conf`` on most
systems.  The default policy provided by OpenVPN 3 Linux allows all users on the
system to load, start and manage their own OpenVPN configuration profiles and
VPN sessions.

All D-Bus services are started on-demand and will also exit automatically if
being idle for too long.  How long differs between each service.  The
auto-start configurations can usually be found in these files:
``/usr/share/dbus-1/system-services/net.openvpn.v3.*.service``.


COMPATIBILITY WITH OPENVPN 2.x
==============================

OpenVPN 3 is mostly compatible with most of the client options found in
OpenVPN 2.x.  But there are some important details where it differs.

* No TAP support

  OpenVPN 3 does not support TAP interfaces, only TUN interfaces.  This is
  by design, as all OS platforms which provides a VPN API (Apple iOS,
  macOS, Android, Microsoft Universal Windows Platform) does only support
  TUN interfaces.

* ``--fragment`` is not supported

  While the use-cases for this feature is acknowledged, it is not
  considered an easy feature to implement and it is being worked on
  alternatives which gives a reasonably equivalent user experience without
  using ``--fragment`` where this will be automatically configured.  If it
  turns out an alternative approach is impossible, this feature might be
  implemented in the end.

* Currently no PKCS#11 support

  This is a bigger task which will be worked on later, but it will attempt
  to integrate better on the overall Linux platform instead of providing
  an OpenVPN specific implementation.

These are the most obvious features users will notice.  This list might be
extended as we receive more feedback.


SEE ALSO
========
``openvpn2``\(1)
``openvpn3``\(1)
``openvpn3-autoload``\(8)
``openvpn3-service-backendstart``\(8)
``openvpn3-service-client``\(8)
``openvpn3-service-configmgr``\(8)
``openvpn3-service-logger``\(8)
``openvpn3-service-netcfg``\(8)
``openvpn3-service-sessionmgr``\(8)
``openvpn3-systemd``\(8)
