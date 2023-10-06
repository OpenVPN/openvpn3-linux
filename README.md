OpenVPN 3 Linux
===============

OpenVPN 3 Linux is an OpenVPN platform which builds on capabilities
available on newer Linux distributions.  Compared to the more classic
OpenVPN 2.x generation, OpenVPN 3 Linux covers many more aspects of the
VPN configuration and session life-cycle than prior OpenVPN generations did.

To quickly compare them, OpenVPN 2.x provides a single executable which
is responsible for a single VPN session.  There are no configuration or
session management in OpenVPN 2.x itself, it depends on the systemd
`openvpn-client@.service` and `openvpn-server@.service` unit files, the
Network Manager OpenVPN plug-in or other third-party management tools.

OpenVPN 3 Linux provides full configuration and session management in
addition to providing the VPN tunnel itself.  For example, it has built in
privilege separation and execution models, for improved process security.
This allows unprivileged users to start their own VPN sessions and manage
them themselves.  VPN configuration profiles can be shared with other users
on the system or kept private.  All without installing anything
additionally.

Through this privilege separation model, the network configuration aspect
of the VPN tunnel is split out into its own process which runs with as few
privileges as possible.  In practice that means it can only do network
configuration changes.  This process knows nothing about the connection
to the VPN server, it just facilitates creating the virtual network adapter
and configuring it with network routes.  This network configuration service
is also capable of setting up the DNS resolver out-of-the-box.  For
OpenVPN 2.x to do that, it would need to run additional scripts or use
specific plug-ins to trigger such updates on the system.

The same OpenVPN 3 Core library which is used in the OpenVPN Connect
clients is also used in this project.  This implementation does not support
all options OpenVPN 2.x does, but if you have a functional configuration
with OpenVPN Connect (typically on Android or iOS devices) it should work
with this client.  In general OpenVPN 3 supports routed TUN configurations;
TAP and bridged setups are not supported and will not work.

The OpenVPN 3 Linux architecture is based on splitting up the functionality
into several independently running services.  They are referred to as
*backend services*.  The interaction with these services happens through
what is referred to as a *user front-end*.  This project also ships
with a Python 3 module which can be used to implement your own OpenVPN
front-ends.

On a more technical level, the integration between the *user front-end* and
the *backend services* is built on top of D-Bus.  Any programming language
supporting D-Bus can also be used to extend and implement a richer
functionality.


Pre-built binaries
-----------------

See the instructions on
https://community.openvpn.net/openvpn/wiki/OpenVPN3Linux how to install
pre-built OpenVPN 3 Linux packages on Debian, Ubuntu, Fedora,
Red Hat Enterprise Linux, CentOS and Scientific Linux.


Getting started using OpenVPN 3 Linux
-------------------------------------

See the [QUICK-START](QUICK-START.md) document to get started using
OpenVPN 3 Linux.


Introduction to the OpenVPN 3 Linux architecture
------------------------------------------------

To interact with the various OpenVPN 3 services running in the background,
three different utilities are provided.

* `openvpn2`
  ([man page](docs/man/openvpn2.1.rst))

  This is an interface which tries to look and behave more like the classic
  OpenVPN 2.x versions.  It does only allow options which are supported by
  the OpenVPN 3 Core Library, plus there are a handful options which are
  ignored as it is possible to establish connections without those options
  active.  Only client side options are supported.

  When running openvpn2 with `--daemon` it will return a D-Bus session path
  to the VPN session.  This path can be used by the `openvpn3` utility to
  further manage this session.

* `openvpn3`
  ([man page](docs/man/openvpn3.1.rst))

  This is a brand new command line interface which does not look like
  OpenVPN 2.x at all.  It can be used to start, stop, pause, resume tunnels
  and retrieve tunnel statistics.  It can also be used as import, retrieve
  and manage configurations stored in the configuration manager, as well as
  handling access control lists for VPN configuration profiles and running
  VPN sessions.

* `openvpn3-admin`
  ([man page](docs/man/openvpn3-admin.8.rst))

  This will mostly only work when run as `root`.  This is used to adjust
  some settings or retrieve information for some of the backend services
  and related system administration tasks.

As mentioned earlier, the OpenVPN 3 Linux project is built on top of D-Bus.
This provides an API which can be used to further interact with the
OpenVPN 3 Linux stack.  It can be used to create a new user front-end or
it can be used to trigger other operations on the host when certain events
happens.

The OpenVPN 3 Linux stack consists of several D-Bus services running in the
background.  There are six services which is good to beware of.  All of
these services will normally start automatically.  And when they are idle
for a while with no data to maintain, they will shut-down automatically.

* `openvpn3-service-configmgr`
   ([man page](docs/man/openvpn3-service-configmgr.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.configuration.md))

  This is the configuration manager.  All configuration profiles will be
  uploaded to and managed by this service before a tunnel is started.  This
  service also ensures only users granted access to imported VPN profiles
  has the proper access to them.  By default this process is started as
  the `openvpn` user.

* `openvpn3-service-sessionmgr`
  ([man page](docs/man/openvpn3-service-sessionmgr.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.sessions.md))

  This manages all VPN tunnels which are about to start or has started.  It
  takes care of communicating with the VPN backend processes and ensures
  only users with the right access levels can manage the various tunnels.
  This service is started as the `openvpn` user.

* `openvpn3-service-backendstart`
  ([man page](docs/man/openvpn3-service-backendstart.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.backends.md))

  This is a helper service and is only availble for the session manager.
  The only task this service has is to start a new VPN client backend
  processes (the VPN tunnel instances).  By default this is also started
  as the `openvpn` user.

* `openvpn3-service-client`
  ([man page](docs/man/openvpn3-service-client.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.client.md))

  This must be started by the `openvpn3-service-backendstart` service.  One
  such process is started per VPN client.  Once it has started, it registers
  itself with the session manager and the session manager provides it with
  the needed details so it can retrieve the proper configuration profile
  from the configuration manager.  This service will depend on the
  `openvpn3-service-netcfg` to manage the tun interface and related
  configuration.  This service is started as the `openvpn` users.

* `openvpn3-service-netcfg`
  ([man page](docs/man/openvpn3-service-netcfg.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.netcfg.md))

  This provides a service similar to a VPN API on some platforms.  It
  is responsible for creating, managing and destroying of virtual tunnel
  interfaces, such as the `tun` or `ovpn`  Data Channel Offload interfaces.
  It will also configure them in addition to handle the DNS configuration
  provided by the VPN server.  This is the most privileged process which
  only have a few capabilities enabled (such as `CAP_NET_ADMIN` and
  possibly `CAP_DAC_OVERRIDE` or `CAP_NET_RAW`).  With these capabilities,
  the service can run as the `openvpn` user.

  Currently DNS configuration is done by manipulating `/etc/resolv.conf`
  directly.  Support for `systemd-resolved` has been added.  On Linux
  distrubutions expected to be pre-configured with `systemd-resolved`,
  OpenVPN 3 Linux will use this service.  On other distributions this need
  to be enabled manually by running the following command as `root`:

      # openvpn3-admin netcfg-service --config-set systemd-resolved true

  Next time the ``openvpn3-service-netcfg`` service restarts,
  `systemd-resolved` support will be used instead.  Note, this requires at
  least **systemd v243** or newer (or a distribution which has back-ported
  a newer version).  This works now with CentOS 8, Fedora 31 and newer,
  Red Hat Enterprise Linux 8 or Ubuntu 20.04 and newer.

  To disable the `systemd-resovled` integration and use `/etc/resolv.conf`
  instead, run these commands as `root`:

      # openvpn3-admin netcfg-service --config-unset systemd-resolved
      # openvpn3-admin netcfg-service --config-set resolv-conf /etc/resolv.conf

* `openvpn3-service-logger`
  ([man page](docs/man/openvpn3-service-logger.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.log.md))

  This service will listen for log events happening from all the various
  services in the OpenVPN 3 Linux stack.  It supports writing these events
  to the console (stdout), files or redirect to syslog or the
  `systemd-journald`.  This is also automatically started  when needed, if
  it isn't already running.

More information can be found in the [`openvpn3-linux(7)`](docs/man/openvpn3-linux.7.rst)
man page and [OpenVPN 3 D-Bus overview](docs/dbus/dbus-overview.md).



####  Kernel based Data Channel Offload (DCO) support

The Data Channel Offload support moves the processing of the OpenVPN data
channel operations from the client process to the kernel, via the ovpn-dco-v2
kernel module.  This means the encryption and decryption of the tunnelled
network traffic is kept entirely in kernel space instead of being send
back and forth between the kernel and the OpenVPN client process.  This
has the potential to improve the overall VPN throughput.  This module must
be installed before OpenVPN 3 Linux can make use of this feature.  This is
shipped in the OpenVPN 3 Linux package repositories or can be built from
the [source code](https://gitlab.com/openvpn/ovpn-dco/).

The ovpn-dco kernel module currently only support ***Linux kernel 5.4***
and newer.  Currently supported distributions with DCO support:

 * CentOS 8
 * Fedora 36 and newer
 * Red Hat Enterprise Linux 8 and newer
 * Ubuntu 20.04 and newer

The ovpn-dco-v2 kernel module is currently not functional on RHEL/CentOS
due to the kernel version is older than 4.18.  OpenVPN 3 Linux will build
with the ``--enable-dco`` feature but requires a functional ``ovpn-dco``
kernel module to be fully functional.

To build OpenVPN 3 Linux with this support, add ``--enable-dco`` to the
``./configure`` command.


#### SELinux support

The `openvpn3-service-netcfg` service depends on being able to pass a file
descriptor to the tun device it has created on behalf of the
`openvpn3-service-client` service (where each of these processes represents
a single VPN session).  This is done via D-Bus.  But on systems with
SELinux, the D-Bus daemon is not allowed to pass file descriptors related
to `/dev/net/tun`.

The OpenVPN 3 Linux project ships two SELinux policy modules, which will be
installed in `/usr/share/selinux/packages`.

The `openvpn3.pp` policy package adds a SELinux boolean,
`dbus_access_tuntap_device`, which grants processes, such as `dbus-daemon`
or `dbus-broker` daemon (running under the `system_dbusd_t` process context)
access to files labelled as `tun_tap_device_t`; which matches the label of
`/dev/net/tun`.  Without this policy enabled, the `openvpn3-service-netcfg`
service will not be able to create or manage TUN devices.

To install and activate this SELinux security module, as root run:

         # semodule -i /etc/openvpn3/selinux/openvpn3.pp
         # semanage boolean --m --on dbus_access_tuntap_device

For users installing the pre-built RPM binaries, this is handled by the RPM
scriptlet during package install.

The second policy module, `openvpn3_service.pp`, will confine both the
`openvpn3-service-netcfg` and `openvpn3-service-client` processes into their
own SELinux process contexts (`openvpn3_netcfg_t` and `openvpn3_client_t`).
See the [`src/selinux/openvpn3_service.te`](src/selinux/openvpn3_service.te)
source for more details.

For the RPM builds, both SELinux policies are provided in the
`openvpn3-selinux` package.


Logging
-------

Logging happens via `openvpn3-service-logger`.  If not started manually,
it will automatically be started by the backend processes needing it.  The
default configuration sends log data to syslog or systemd-journald,
depending on the Linux distribution.  Unless `--syslog`, `--journald`  or
`--log-file` is provided, it will log to the console (stdout).

Real-time log events can be received on a per-session level, by using the
[`openvpn3 log`](docs/man/openvpn3-log.1.rst) command.

This log service is managed via
[`openvpn3-admin log-service`](docs/man/openvpn3-admin-log-service.8.rst.in).
For systems using `systemd-journald`, the
[`openvpn3-admin journal`](docs/man/openvpn3-admin-journal.8.rst) command
provides a convenient approach to retrive only OpenVPN 3 Linux related log
entries from the systemd journal.

For more information about logging, see the
[`openvpn3-service-logger(8)`](docs/man/openvpn3-service-logger.8.rst),
man page, [D-Bus Logging](docs/dbus/dbus-logging.md) and
[`net.openvpn.v3.log` D-Bus service](docs/dbus/dbus-service-net.openvpn.v3.log.md)
documentation.


Debugging
---------

For information about debugging, please see [docs/debugging.md](docs/debugging.md)


Building from source
--------------------

For information about building OpenVPN 3 Linux from source, please
see [BUILD.md](BUILD.md).


Contribution
------------

* Code contributions
  Code contributions are most welcome.  Please submit patches for review
  to the openvpn-devel@lists.sourceforge.net mailing list.  All patches must
  carry a Signed-off-by line and must be reviewed publicly before acceptance.
  Pull requests are not acceptable unless it is for early reviews and patch
  discussions.  Final patches *MUST* go to the mailing list.

* Testing
  This code is quite new, but has been used a lot in various setups.
  Please reach out on libera.chat @ #openvpn for help and discussing issues
  you encounter, or subscribe to and ask on the
  openvpn-users@lists.sourceforge.net mailing list.

* Packagers
  We are beginning to targeting packaging in Linux distributions.  The
  Fedora Copr repository is one which is currently available.  We are
  looking for people willing to package this in other Linux distributions
  as well.
