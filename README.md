OpenVPN 3 Linux client
======================

This is the next generation OpenVPN client for Linux.  This project is very
different from the more classic OpenVPN 2.x versions.  First, this is
currently only a pure client-only implementation.

The biggest change from the classic OpenVPN 2.x generation is that it does
not need to be started by a root or otherwise privileged account any more.
By default, all users on the system will have access to start and manage
their own VPN tunnels.  It will also support configuring DNS out-of-the-box

The same OpenVPN 3 Core library which is used in the OpenVPN Connect clients
is also used in this OpenVPN 3 client.  This implementation does not support
all options OpenVPN 2.x does, but if you have a functional configuration
with OpenVPN Connect (typically on Android or iOS devices) it will work with
this client.  In general OpenVPN 3 supports routed TUN configurations; TAP
and bridged setups are unsupported and will not work.

On a more technical level, this client builds on D-Bus and does also ship
with a Python 3 module which can also be used to implement your own OpenVPN
client front-end.  Any language which supports D-Bus bindings can also be
used.


Pre-built binaries
-----------------

See the instructions on
https://community.openvpn.net/openvpn/wiki/OpenVPN3Linux how to install
pre-built OpenVPN 3 Linux packages on Debian, Ubuntu, Fedora,
Red Hat Enterprise Linux, CentOS and Scientific Linux.


Quick start: Using the `openvpn2` front-end
-----------------------------------------

The `openvpn2` front-end is a command line interface which tries to be
similar to the old and classic openvpn-2.x generation.  It supports most of
the options used by clients and will ignore unsupported options which does
not impact the ability to get a connection running.

* Starting a VPN session:

      $ openvpn2 --config my-vpn-config.conf

If the provided configuration contains the `--daemon` option, it will
provide the session path related to this session and return to the command
line again.  From this point of, this session needs to be managed via the
`openvpn3` front-end.

For more information, see the [`openvpn2(1)`](docs/man/openvpn2.1.rst) and
[`openvpn3-session-manage(1)`](docs/man/openvpn3-session-manage.1.rst)
man-pages.


Using the openvpn3 front-end
----------------------------

The `openvpn3` program is the main and preferred command line user interface.

* Starting a VPN session: Single-shot approach

      $ openvpn3 session-start --config my-vpn-config.conf

  This will import the configuration and start a new session directly

* Starting a VPN session: Multi-step approach

  1. Import the configuration file:

         $ openvpn3 config-import --config my-vpn-config.conf

      This will return a configuration path.  This path is a unique reference
      to this specific configuration profile.

  2. (Optional) Display all imported configuration profiles

         $ openvpn3 configs-list

  3. Start a new VPN session

         $ openvpn3 session-start --config my-vpn-config.conf

     or

         $ openvpn3 session-start --config-path /net/openvpn/v3/configuration/d45d4263x42b8x4669xa8b2x583bcac770b2

* Listing established sessions

         $ openvpn3 sessions-list

* Getting tunnel statistics
  For already running tunnels, it is possible to extract live statistics
  of each VPN session individually

      $ openvpn3 session-stats --config my-vpn-config.conf

    or

      $ openvpn3 session-stats --path /net/openvpn/v3/sessions/46fff369sd155s41e5sb97fsbb9d54738124

* Managing VPN sessions
  For running VPN sessions, you manage them using the
  `openvpn3 session-manage` command, again by providing the session path.  For
  example, to restart a connection:

      $ openvpn3 session-manage --config my-vpn-config.conf --restart

    or

      $ openvpn3 session-manage --path /net/openvpn/v3/sessions/46fff369sd155s41e5sb97fsbb9d54738124 --restart

  Other actions can be `--pause`, `--resume`, and `--disconnect`.

All the `openvpn3` operations are also described via the `--help` option.

       $ openvpn3 --help
       $ openvpn3 session-start --help


For more information, see the [`openvpn3(1)`](docs/man/openvpn3.1.rst),
[`openvpn3-session-start(1)`](docs/man/openvpn3-session-start.1.rst),
[`openvpn3-session-manage(1)`](docs/man/openvpn3-session-manage.1.rst) and
[`openvpn3-config-import(1)`](docs/man/openvpn3-config-import.1.rst) man-pages.


Known issues
------------

If OpenVPN 3 Linux fails to start a VPN session, please test with this
command:

     # openvpn3-admin version --services

This should produce the same version string for all services.  If some
of them fails to start, some Linux installations might not have the
`sssd` or `nscd` service running.  Often the `net.openvpn.v3.netcfg`
service (provided  by `openvpn3-service-netcfg`) fails to start properly.
If your system is configured to use `sssd`, please read the comments in
`/etc/nsswitch.conf` carefully if you want to try to start `nscd`.


Auto-loading/starting VPN tunnels
---------------------------------

OpenVPN 3 Linux ships with a [`openvpn3-session@.service`](docs/man/openvpn3-systemd.8.rst)
service unit file to manage VPN sessions via systemd.  This approach
requires configuration profiles to be imported as a persistent
configuration first.  See the
[`openvpn3-systemd(8)`](docs/man/openvpn3-systemd.8.rst) man page for
details.

**NOTE**:
      The `openvpn3-session@.service` unit file approach is not
      available on Red Hat Enterprise Linux 7 and clones, due to
      no available `python3-systemd` package.

Alternatively the older `openvpn3-autoload` utility can be used to pre-load
configuration profiles and possibly also start tunnels.  This requires a little
bit of preparations.  When starting it via `systemctl start openvpn3-autoload`
it will look for configuration profiles found inside `/etc/openvpn3/autoload`
which has a corresponding `.autoload` configuration present in addition.
This tells both the Configuration Manager and Session Manager how to process
the VPN configuration profile.

For more details, look at the [`openvpn3-autoload(8)`](docs/man/openvpn3-autoload.8.rst) man-page.


Introduction to the OpenVPN 3 Linux architecture
------------------------------------------------

To interact with the various OpenVPN 3 services running in the background,
three different utilities are provided.

* `openvpn2`
  ([man page](docs/man/openvpn2.1.rst))

  This is an interface which tries to look and behave a bit more
  like the classic OpenVPN 2.x versions.  It does only allow options which
  are supported by the OpenVPN 3 Core Library, plus there are a handful
  options which are ignored as it is possible to establish connections without
  those options active.

  When running openvpn2 with `--daemon` it will return a D-Bus path to the
  VPN session.  This path can be used by the `openvpn3` utility to further
  manage this session.

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
  some settings or retrieve information from some of the backend services.

The OpenVPN 3 Linux project is built on D-Bus.  This means it is possible to
build your own tools instead of using these tools, all which is required is
to access the various OpenVPN 3 D-Bus services.  In reality all the
front-ends mentioned are just specialized D-Bus clients for the OpenVPN 3
D-Bus services.  This resolves the challenges with proper privilege
separation between users and the various operations running a VPN tunnel
requires.

As mentioned, there are various D-Bus services running behind the scenes.
There are six services which is good to beware of.  All of these services
will normally start automatically.  And when they are idle for a while
with no data to maintain, they will shut-down automatically.

* `openvpn3-service-configmgr`
   ([man page](docs/man/openvpn3-service-configmgr.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.configuration.md))

  This is the configuration manager.  All configuration profiles will be
  uploaded and managed by this service before a tunnel is started.  This
  service also ensures only users granted access to VPN various profiles
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

  This is a helper service and is only used by the session manager.
  The only task this service has is to start a new VPN client backend
  processes (the VPN tunnel instances).  By default this is also started
  as the `openvpn` user.

* `openvpn3-service-client`
  ([man page](docs/man/openvpn3-service-client.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.client.md))

  This is to be started by the `openvpn3-service-backendstart` only.  One such
  process is started per VPN client.  Once it has started, it registers itself
  with the session manager and the session manager provides it with the needed
  details so it can retrieve the proper configuration profile from the
  configuration manager.  This service will depend on the
  `openvpn3-service-netcfg` to manage the tun interface and related
  configuration.  This service is also running as the `openvpn` users by default.

* `openvpn3-service-netcfg`
  ([man page](docs/man/openvpn3-service-netcfg.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.netcfg.md))

  This provides a service similar to a VPN API on other platforms.  It
  is responsible for creating, managing and destroying of TUN interfaces,
  configure them as well as handle the DNS configuration provided by the
  VPN server.  This is the most privileged process which only have a few
  capabilities enabled (such as `CAP_NET_ADMIN` and possibly `CAP_DAC_OVERRIDE`
  or `CAP_NET_RAW`).  With these capabilities, the service can run
  as the `openvpn` user.

  Currently DNS configuration is done by manipulating `/etc/resolv.conf`
  directly.  Support for systemd-resolved has been added, and will require
  enabling this feature by running the following command as root:

      # openvpn3-admin netcfg-service --config-set systemd-resolved 1

  Next time the ``openvpn3-service-netcfg`` service restarts, systemd-resolved
  support will be used instead.  Note, this requires at least **systemd v243**
  or newer (or a distribution which has back-ported a newer version).  This works
  now with CentOS 8, Fedora 31 and newer, Red Hat Enterprise Linux 8 or
  Ubuntu 20.04.

* `openvpn3-service-logger`
  ([man page](docs/man/openvpn3-service-logger.8.rst) | [D-Bus documentation](docs/dbus/dbus-service-net.openvpn.v3.log.md))

  This service will listen for log events happening from all the various
  OpenVPN 3 D-Bus services.  It supports writing these events to the console
  (stdout), files or redirect to syslog.  This is also automatically started
  when needed, if it isn't already running.

More information can be found in the [`openvpn3-linux(7)`](docs/man/openvpn3-linux.7.rst)
man page and [OpenVPN 3 D-Bus overview](docs/dbus/dbus-overview.md).


How to build openvpn3-linux locally
-----------------------------------

The primary Linux distributions targeted and regularly tested are:

  - CentOS 7 and 8
  - Debian 10 and 11
  - Fedora 35, 36 and Rawhide
  - Red Hat Enterprise Linux (RHEL) 7 and 8
  - Scientific Linux 7
  - Ubuntu 18.04, 20.04 and 22.04

This list is not an exclusive list, and it will most likely work
on all other distributions with recent enough dependencies.

The following dependencies are needed:

* A C++ compiler capable of at least ``-std=c++11``.  The ``./configure``
  script will try to detect if ``-std=c++14`` is available and switch to
  that if possible, otherwise it will test for ``-std=c++11``.  If support
  for neither is found, it will fail.  RHEL-7 users may prefer to use
  ``-std=c++1y``.

* GNU autoconf 2.69 or newer

  https://www.gnu.org/software/autoconf/

* GNU autoconf-archive (2017.03.21 has been tested)

  https://www.gnu.org/software/autoconf-archive/

* GNU automake 1.11 or newer

  https://www.gnu.org/software/automake/

* OpenSSL 1.0.2 or newer (recommended, not needed if building with mbed TLS)

  https://www.openssl.org/

* mbed TLS 2.13 or newer  (not needed if building with OpenSSL)

  https://tls.mbed.org/

* GLib2 2.50 or newer

  http://www.gtk.org
  This dependency is due to the GDBus library, which is the D-Bus
  implementation being used.

* jsoncpp 0.10.5 or newer

  https://github.com/open-source-parsers/jsoncpp

* libcap-ng 0.7.5 or newer

  http://people.redhat.com/sgrubb/libcap-ng

* liblz4 1.7.3 or newer

  https://lz4.github.io/lz4

* libuuid 2.23.2 or newer

  https://en.wikipedia.org/wiki/Util-linux

* polkit 0.105 or newer

  http://www.freedesktop.org/wiki/Software/polkit

  Only needed when using the systemd-resolved integration.  On Ubuntu this
  package is called policykit-1.

* (optional) libnl3 3.2.29 or newer

  http://www.infradead.org/~tgr/libnl/

  Only needed when building with DCO support

* (optional) protobuf 2.4.0 or newer

  http://code.google.com/p/protobuf/

  Only needed when building with DCO support

* (optional) tinyxml2 2.1.0 or newer

  https://github.com/leethomason/tinyxml2

  This is only needed if you want to include the AWS-VPC integration.

* (optional) Python 3.5 or newer

  If Python 3.5 or newer is found, the openvpn2, openvpn3-autoload utilities
  and the openvpn3 Python module will be built and installed.

* (optional) Python docutils

  http://docutils.sourceforge.net/
  This is needed for the `rst2man` utility, used to generate the man pages.

* (optional) Python Jinja2 template engine

   https://palletsprojects.com/p/jinja/
   Required when enabling the bash-completion support; used to generate the
   bash-completion script for openvpn2.

* (optional) selinux-policy-devel

  For Linux distributions running with SELinux in enforced mode (like Red Hat
  Enterprise Linux and Fedora), this is required.

In addition, this git repository will pull in these git submodules:

* openvpn3

  https://github.com/OpenVPN/openvpn3
  This is the OpenVPN 3 Core library.  This is where the core
  VPN implementation is done.

* ASIO

  https://github.com/chriskohlhoff/asio
  The OpenVPN 3 Core library depends on some bleeding edge features
  in ASIO, so we need to do a build against the ASIO git repository.

  This openvpn3-linux git repository will pull in the appropriate ASIO
  library as a git submodule.

* googletest

  https://github.com/google/googletest
  Used by the OpenVPN 3 Linux unit-tests

First install the package dependencies needed to run the build.

#### Debian/Ubuntu:

- Building with OpenSSL (recommended):

  For newer Debian and Ubuntu releases shipping with OpenSSL 1.1 or newer:

      # apt-get install libssl-dev libssl1.1

  For Ubuntu 16.04 LTS, which ships with OpenSSL 1.0:

      # apt-get install libssl-dev libssl1.0.0

- Building with mbed TLS (alternative):

      # apt-get install libmbedtls-dev

- Additional Debian 9 and Ubuntu 16.04 Python requirements

  The ``openvpn3`` Python module requires the ``IntFlag`` extension in the
  `enum`` module.  This was introduced in the Python 3.6 distribution.
  OpenVPN 3 Linux implements a workaround for this in distributions which can
  install the ``aenum`` module via ``pip3``

      # apt-get install python3-pip
      # pip3 install aenum

- Generic build requirements:

      # apt-get install build-essential git pkg-config autoconf autoconf-archive libglib2.0-dev libjsoncpp-dev uuid-dev liblz4-dev libcap-ng-dev libxml2-utils python3-minimal python3-dbus python3-docutils python3-jinja2 libxml2-utils libtinyxml2-dev policykit-1 libsystemd-dev python3-systemd

- Dependencies to build with DCO support:

      # apt-get install libnl-3-dev libnl-genl-3-dev protobuf-compiler libprotobuf-dev


#### Fedora:

- Building with OpenSSL (recommended):

      # yum install openssl-devel

- Building with mbed TLS (alternative):

      # yum install mbedtls-devel

- Generic build requirements:

      # yum install gcc-c++ git autoconf autoconf-archive automake make pkgconfig glib2-devel jsoncpp-devel libuuid-devel libcap-ng-devel selinux-policy-devel lz4-devel zlib-devel libxml2 tinyxml2-devel python3-dbus python3-gobject python3-pyOpenSSL python3-jinja2 python3-docutils bzip2 polkit systemd-devel python3-systemd

- Dependencies to build with DCO support:

      # yum install libnl3-devel protobuf-compiler protobuf protobuf-devel


#### Red Hat Enterprise Linux / CentOS / Scientific Linux
  First install the ``epel-release`` repository if that is not yet installed.

##### NOTES: RHEL 8 / CentOS 8 - differences from RHEL 7 / CentOS 7
  For Red Hat Enterprise Linux 8 and CentOS 8, use the package lists used in
  Fedora.  In addition the CodeReady (RHEL) / PowerTools (CentOS) repository
  needs to be enabled when building with Data Channel Offload (DCO) support.

  For CentOS 8 run this command:

     # yum -y install yum-utils
     # yum-config-manager --set-enabled powertools

##### Required packages

- (Optional) Installing ``tinyxml2`` and ``tinyxml2-devel``
  If you want to use ``--enable-addons-aws``, you will need the tinyxml2
  development packages.  If you cannot find these packages in your normal
  repositories, they are also available in the ``openvpn3`` Fedora Copr
  repository.  These packages are also found in the CodeReady (RHEL) and
  PowerTools (CentOS) repositories.

      # yum install tinyxml2 tinyxml2-devel

- Building with OpenSSL (recommended)

      # yum install openssl-devel

- Building with mbed TLS (alternative):

      # yum install mbedtls-devel

- Generic build requirements (only RHEL 7):

      # yum install gcc-c++ git autoconf autoconf-archive automake make pkgconfig glib2-devel jsoncpp-devel libuuid-devel lz4-devel libcap-ng-devel selinux-policy-devel lz4-devel zlib-devel libxml2 python-docutils python36 python36-dbus python36-gobject python36-pyOpenSSL polkit systemd-devel

  For RHEL 8/CentOS 8 see the Fedora package lists, including dependencies for DCO support.


### Preparations building from git
- Clone this git repository: ``git clone git://github.com/OpenVPN/openvpn3-linux``
- Enter the ``openvpn3-linux`` directory: ``cd openvpn3-linux``
- Run: ``./bootstrap.sh``

Completing these steps will provide you with a `./configure` script.


### Adding the `openvpn` user and group accounts
The default configuration for the services assumes a service account
`openvpn` to be present.  If it does not exist you should add one, e.g. by:

    # groupadd -r openvpn
    # useradd -r -s /sbin/nologin -g openvpn openvpn


### Building OpenVPN 3 Linux client
If you already have a `./configure` script or have retrieved an
`openvpn3-linux-*.tar.xz` tarball generated by `make dist`, the following steps
will build the client.

- Run: ``./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var``
- Run: ``make``
- Run: ``make install``

By default, OpenVPN 3 Linux is built using the OpenSSL library.  If you want
to compile against mbed TLS, add the ``--with-crypto-library=mbedtls`` argument
to ``./configure``.

You might need to also reload D-Bus configuration to make D-Bus aware of
the newly installed service.  On most system this happens automatically
but occasionally a manual operation is needed:

    # systemctl reload dbus

The ``--prefix`` can be changed, but beware that you will then need to add
``--datarootdir=/usr/share`` instead.  This is related to the D-Bus auto-start
feature.  The needed D-Bus service profiles will otherwise be installed in a
directory the D-Bus message service does not know of.  The same is for the
``--sysconfdir`` path.  It will install a needed OpenVPN 3 D-Bus policy into
``/etc/dbus-1/system.d/``.

With everything built and installed, it should be possible to run both the
``openvpn2`` and ``openvpn3`` command line tools - even as an unprivileged
user.


#### Enable AWS-VPC integration

If you want to enable the AWS-VPC integration, add ``--enable-addons-aws``
to the ``./configure`` command.


#### TECH PREVIEW: Kernel based Data Channel Offload (DCO) support

**BEWARE - UNDER HEAVY DEVELOPMENT**

     This feature is under heavy development.  It is NOT production
     ready and the API between the kernel module and OpenVPN 3 Linux may
     change in incompatible ways for the time being until the API is
     considered stable.

The Data Channel Offload support moves the processing of the OpenVPN data
channel operations from the client process to the kernel, via the ovpn-dco
kernel module.  This means the encryption and decryption of the tunnelled
network traffic is kept entirely in kernel space instead of being send
back and forth between the kernel and the OpenVPN client process.  This
has the potential to improve the overall VPN throughput.  This module must
be installed before OpenVPN 3 Linux can make use of this feature.  This is
shipped in the OpenVPN 3 Linux package repositories or can be built from
the [source code](https://gitlab.com/openvpn/ovpn-dco/).

The ovpn-dco kernel module currently only support ***Linux kernel 5.4*** and
newer.  Currently supported distributions with DCO support:

 * CentOS 8
 * Fedora release 33, 34 and Rawhide
 * Red Hat Enterprise Linux 8
 * Ubuntu 20.04 and newer

The ovpn-dco kernel module is currently not functional on RHEL/CentOS due
to the kernel version is older than 4.18.  OpenVPN 3 Linux will build with
the ``--enable-dco`` feature but requires a functional ``ovpn-dco``
kernel module to be fully functional.

To build OpenVPN 3 Linux with this support, add ``--enable-dco`` to the
``./configure`` command.


#### Auto-completion helper for bash/zsh

If you want to also install the bash-completion scripts for the
``openvpn2`` and ``openvpn3`` commands, add ``--enable-bash-completion``
to the ``./configure`` command.


#### SELinux

The `openvpn3-service-netcfg` service depends on being able to pass a file
descriptor to the tun device it has created on behalf of the
`openvpn3-service-client` service (where each of these processes represents
a single VPN session).  This is done via D-Bus.  But on systems with
SELinux, the D-Bus daemon is not allowed to pass file descriptors related
to `/dev/net/tun`.

The openvpn3-linux project ships two SELinux policy modules, which will be
installed in `/usr/share/selinux/packages` **if** the `./configure` script can
locate the SELinux policy development files.  On RHEL/Fedora the development
files are located under `/usr/share/selinux/devel` and provided by the
`selinux-policy-devel` package.

If the `selinux-policy-devel` package has been detected by `./configure`,
running `make install` will install the `openvpn3.pp` policy package,
typically in `/usr/share/selinux/packages`.

The `openvpn3.pp` policy package adds a SELinux boolean, `dbus_access_tuntap_device`,
which grants processes, such as `dbus-daemon` running under the
`system_dbusd_t` security context access to files labelled as
`tun_tap_device_t`; which matches the label of `/dev/net/tun`.

To install and activate this SELinux security module, as root run:

         # semodule -i /etc/openvpn3/selinux/openvpn3.pp
         # semanage boolean --m --on dbus_access_tuntap_device

On Red Hat Enterprise Linux and Fedora, the `openvpn3-service-netcfg` will
stop running and the OpenVPN 3 Linux client will be non-functional if this
has not been done.  The source code of the policy package can be found in
[`src/selinux/openvpn3.te`](src/selinux/openvpn3.te).

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
default configuration sends log data to syslog.  This service can be
started manually and must run as the `openvpn` user.  If  being started as
`root`, it will automatically switch to the `openvpn` user.  See
`openvpn3-service-logger --help` for more details.  Unless `--syslog` or
`--log-file` is provided, it will log to the console (stdout).

This log service can also be managed (even though fairly few options
to tweak) via `openvpn3-admin log-service`.  The most important feature
here is probably to modify the log level.

For more information about logging, see the
[`openvpn3-service-logger(8)`](docs/man/openvpn3-service-logger.8.rst),
man page, [D-Bus Logging](docs/dbus/dbus-logging.md) and
[`net.openvpn.v3.log` D-Bus service](docs/dbus/dbus-service-net.openvpn.v3.log.md)
documentation.


General debugging
-----------------

Ensure you have done a build using `--enable-debug-options` when running
`./configure`.  This ensures the most crucial debug options are available.

Most of the backend services (`openvpn3-service-logger`,
`openvpn3-service-configmgr`, `openvpn3-service-sessionmgr` and
`openvpn3-service-backendstart`) can be run in a
console.  All with the exception of `openvpn3-service-netcfg` should be
started as the `openvpn` user. `openvpn3-service-netcfg` must be started as
root but will as soon as possible drop its privileges to the `openvpn` user
as well, after it has acquired the `CAP_NET_ADMIN` capability and possibly a
few others.  See their corresponding `--help` screen for details.  Most of
these programs can be forced to provide more log data by setting
`--log-level`.  And they can all provide logging to the console.

For more information about debugging, please see
[docs/debugging.md](docs/debugging.md)


D-Bus debugging
---------------
To debug what is happening, ``busctl``, ``gdbus`` and ``dbus-send``
utilities are useful.  The service destinations these tools need to move
forward are:

- net.openvpn.v3.configuration (Configuration manager)
- net.openvpn.v3.sessions (Session manager)

Both of these services allows introspection.

There exists also a net.openvpn.v3.backends service, but that is restricted
to be accessible only by the openvpn user - and even that users access is
locked-down by default and introspection is not possible without modifying
the D-Bus policy.

Looking at the D-Bus log messages can be also helpful, for example with:

    $ journalctl --since today -u dbus

For more information about debugging, please see
[docs/debugging.md](docs/debugging.md)


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



DISCLAIMER
----------

The OpenVPN 3 Linux project is BETA quality.  It is fully functional and
so far we have few reports about instabilities.

The OpenVPN 3 Core library this project builds on is used by the
OpenVPN Connect and Private Tunnel clients in addition to the
OpenVPN for Android client (need to explicitly enable the OpenVPN 3 backend),
so the pure VPN tunnel implementation should be good to use.
