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
the more classic OpenVPN 2.x generation.  Where OpenVPN 2.x is very simplistic
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

OPENVPN 3 D-BUS SERVICES
========================

* *Configuration Manager* (``openvpn3-service-configmgr``)

        This provides a D-Bus service which only takes care of managing VPN
        configuration profiles for all users on the system.  It has a built in
        Access Control mechanism, which ensures only users granted access can
        access the information they should have.

* *Session Manager* (``openvpn3-service-sessionmgr``)

        This provides a D-Bus service which only takes care of managing VPN
        sessions on the system.  This has also its own set of Access Control
        mechanisms, which restricts access to VPN sessions might be running in
        parallel across users accounts.

* *Backend VPN Client* (``openvpn3-service-client``)

        This is the core VPN client, where it will exist one such process per
        active VPN session.

* *Backend VPN Client Starter* (``openvpn3-service-backendstart``)

        This is is a D-Bus service which starts automatically when a new Backend
        VPN Client process is needed.  This is only used by the Session Manager.

* *Network Configuration* (``openvpn3-service-netcfg``)

        This is the most privileged D-Bus service, and is only used by the
        Backend VPN Client processes.  This is responsible for creating and
        manage virtual network interfaces, configure IP addresses, routes in
        addition to handling the DNS setup.

* *Logging* (``openvpn3-service-logger``)

        This is responsible to receive log events from all the services above
        and ensure they get logged properly.  Whether that is to a terminal, a
        log file or syslog, depends on the configuration of it.  The default is
        to start this service using the syslog integration.

All of these services above are D-Bus services.  They are all automatically
started when needed, with the bare minimum of privileges they need.


USER FRONT-ENDS
===============

To make use of these D-Bus services, the OpenVPN 3 Linux project ships with
three different front-ends targeting users.

* ``openvpn2``

        This aims to be fairly similar to the more classic ``openvpn``\(8)
        command line, with the exception that it only processes client related
        options.

* ``openvpn3``

        This is the main utility for managing all the features available in
        OpenVPN 3 Linux and has a very different user interface than
        ``openvpn2``\(1) or ``openvpn``\(8).  On systems with the
        bash-completion integration installed, almost every command, option and
        argument used by ``openvpn3`` will get auto-completion help.

* ``openvpn3-autoload``

        This is not strictly a utility most users will use too often.  But it
        can be used to automatically load VPN configuration profiles as well as
        starting them during boot or login.

In addition, a Python 3 module is also available making it possible to write
your own OpenVPN 3 front-ends with little extra efforts.  In fact, both
``openvpn2`` and ``openvpn3-autoload`` are just Python 3 scripts.


D-BUS POLICY
============
D-Bus enforces usage of a policy which defines who can access which D-Bus
service and which features, objects and properties provided by a D-Bus service.
This is defined in ``/etc/dbus-1/systemd.d/net.openvpn.v3.conf`` on most
systems.  The default policy provided by OpenVPN 3 Linux allows all users on the
system to load, start and manage their own OpenVPN configuration profiles and
VPN sessions.

All D-Bus services are started on-demand and will also exit automatically if
being idle for too long.  How long differs quite a bit between
each service.  The auto-start configurations can usually be found in
these files: ``/usr/share/dbus-1/system-services/net.openvpn.v3.*.service``.


COMPATIBILITY WITH OPENVPN 2.x
==============================
OpenVPN 3 is mostly compatible with most of the client options found in
OpenVPN 2.x.  But there are some important details where it differs.

* No TAP support

        OpenVPN 3 does not support TAP interfaces, only TUN interfaces.  This is
        by design, as all OS platforms which provides a VPN API (Apple iOS,
        macOS, Android, Microsoft Universal Windows Platform) does only support
        TUN interfaces.

* ``--fragment`` is unsupported

        While the use-cases for this feature is acknowledged, it is not
        considered an easy feature to implement and it is being worked on
        alternatives which gives a reasonably equivalent user experience without
        using ``--fragment`` where this will be automatically configured.  If it
        turns out an alternative approch is impossible, this feature might be
        implemented in the very end.

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
