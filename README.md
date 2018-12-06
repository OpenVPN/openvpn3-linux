OpenVPN 3 Linux client
======================

This is the next generation OpenVPN client for Linux.  This project is very
different from the more classic OpenVPN 2.x versions.  First of all, this is
currently only a pure client-only implementation.

This client depends on D-Bus to function.  The implementation tries to resolve
a lot of issues related to privilege separation and that the VPN tunnel can
still access information needed by the front-end user which starts a tunnel.

All of the backend services will normally start automatically.  And when they
are lingering idle for a little while with no data to maintain, they will
shut-down automatically.

There are six services which is good to beware of:

* openvpn3-service-configmgr

  This is the configuration manager.  All configurations will be uploaded to
  this service before a tunnel is started.  This process is started as the
  openvpn user.

* openvpn3-service-sessionmgr

  This manages all VPN tunnels which is about to start or has started.  It
  takes care of communicating with the backend tunnel processes and ensures
  only users with the right access levels can manage the various tunnels.
  This service is started as the openvpn user.

* openvpn3-service-backendstart

  This is more or less a helper service and is only used by the session manager
  The only task this service has is to start a new VPN client backend processes
  (the VPN tunnel instances)

* openvpn3-service-client

  This is to be started by the openvpn3-service-backend only.  One such
  process is started per VPN client.  Once it has started, it registers itself
  with the session manager and the session manager provides it with the needed
  details so it can retrieve the proper configuration from the configuration
  manager.  This service will depend on the openvpn3-service-netcfg to manage
  the tun interface and related configuration.

* openvpn3-service-netcfg

  This provides a service similar to a VPN API on other platforms.  It is
  responsible for creating, managing and destroying of tun interfaces, configure
  them as well as handle the DNS configuration provided by the VPN server.
  This is the most privileged process which only have a few capabilities
  enabled (such as `CAP_NET_ADMIN` and possibly `CAP_DAC_OVERRIDE` or
  `CAP_NET_RAW`).  With these capabilities, the service can run as the
  openvpn user.  Currently DNS configuration is done by manipulating
  `/etc/resolv.conf` directly, but can be extended to support better methods
  (systemd-resolved and NetworkManager are being investigated as potential
  solutions).  When integrating with other services, the `CAP_DAC_OVERRIDE`
  privilege might not be needed.  The `CAP_NET_RAW` capability is only needed
  when using `--redirect-method bind-device`.

* openvpn3-service-logger

  This service will listen for log events happening from all the various
  backend services.  Supports writing to the console (stdout), files or
  redirect to syslog.  This is also automatically started when needed, if
  it isn't already running.


To interact with these services, there are two front-end tools provided:

* openvpn3

  This is a brand new command line interface which does not look like
  OpenVPN 2.x at all.  It can be used to start, stop, pause, resume tunnels
  and retrieve tunnel statistics. It can also be used as import, retrieve
  and manage configurations stored in the configuration manager and retrieve
  tunnel statistics for running tunnels.

* openvpn2

  This is a simpler interface which tries to look and behave a bit more
  like the classic OpenVPN 2.x versions.  This interface is written in
  Python.  It does only allow options which are supported by the OpenVPN 3
  Core library, plus there are a handful options which are ignored as it
  is possible to establish connections without those options active.

  When running openvpn2 with `--daemon` it will return a D-Bus path to the
  VPN session.  This path can be used by the `openvpn3` utility to further
  manage this session.


Using the openvpn3 front-end
----------------------------

The `openvpn3` program is the main and preferred command line user interface.

* Starting a VPN session: Single-shot approach

      $ openvpn3 session-start --config my-vpn-config.conf

  This will import the configuration and start a new session directly

* Starting a VPN session: Multi-step approach

  1. Import the configuration file:

         $ openvpn3 config-import --config my-vpn-config.conf

      This will return a configuration path.  This is needed to interact
      with this configuration later on.

  2. (Optional) Display all imported configuration profiles

         $ openvpn3 configs-list

  3. Start a new VPN session

         $ openvpn3 session-start --config-path /net/openvpn/v3/configuration/d45d4263x42b8x4669xa8b2x583bcac770b2

* Listing established sessions

         $ openvpn3 sessions-list

* Getting tunnel statistics
  For already running tunnels, it is possible to extract live statistics
  of each VPN session individually

      $ openvpn3 session-stats --path /net/openvpn/v3/sessions/46fff369sd155s41e5sb97fsbb9d54738124

* Managing VPN sessions
  For running VPN sessions, you manage them using the
  `openvpn3 session-manage` command, again by providing the session path.  For
  example, to restart a connection:

      $ openvpn3 session-manage --path /net/openvpn/v3/sessions/46fff369sd155s41e5sb97fsbb9d54738124 --restart

  Other actions can be `--pause`, `--resume`, and `--disconnect`.

All the `openvpn3` operations are also described via the `--help` option.

       $ openvpn3 --help
       $ openvpn3 session-start --help


Using openvpn2 front-end
------------------------

The `openvpn2` front-end is a simpler interface which tries to be somewhat
similar to the old and classic openvpn-2.x generation.  It supports most of
the options used by clients, but not everything.  This might also be a
limitation of the OpenVPN 3 Core library this openvpn3-linux client builds
on.

* Starting a VPN session:

      $ openvpn2 --config my-vpn-config.conf

If the provided configuration contains the `--daemon` option, it will
provide the session path related to this session and return to the command
line again.  From this point of, this session is now to be managed via the
`openvpn3` front-end.


Auto-loading/starting VPN tunnels
---------------------------------

To enable loading configuration profiles and possibly also start them
automatically, the `openvpn3-autoload` tool will handle this.  But it
requires a little bit of preparations.  By default it will look for
configuration profiles found inside `/etc/openvpn3/autoload` which has
a corresponding `.autoload` configuration present in addition.  This tells
both the Configuration Manager and Session Manager how to process the
VPN configuration profile.  For more details, look at the
[OpenVPN 3 Autoload feature](docs/openvpn3-autoload.md) documentation.


Pre-built binaries
-----------------

There exists installable packages for Fedora, Red Hat Enterprise Linux,
CentOS and Scientific Linux via a Fedora Copr repository.  See this
URL for more details:  https://copr.fedorainfracloud.org/coprs/dsommers/openvpn3/


How to build openvpn3-linux locally
-----------------------------------

The following dependencies are needed:

* A C++ compiler capable of at least ``-std=c++11``.  The ``./configure``
  script will try to detect if ``-std=c++14`` is available and switch to
  that if possible, otherwise it will test for ``-std=c++11``.  If support
  for neither is found, it will fail.

* mbed TLS 2.4 or newer

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

* Python 3.4 or newer (optional)

  If Python 3.4 or newer is found, the openvpn2 utility and an openvpn3
  Python module will be built and installed.

* (optional) Python docutils

  http://docutils.sourceforge.net/
  This is needed for the `rst2man` utility, used to generate the man pages.

* (optional) selinux-policy-devel

  For Linux distributions running with SELinux in enforced mode (like Red Hat
  Enterprise Linux and Fedora), this is required.

In addition, this git repository will pull in two git submodules:

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

First install the package dependencies needed to run the build.

#### Debian/Ubuntu:
  ``# apt-get install build-essential git pkg-config autoconf autoconf-archive libglib2.0-dev libjsoncpp-dev uuid-dev libmbedtls-dev liblz4-dev libcap-ng-dev``

#### Fedora:
  ``# dnf install gcc-c++ git autoconf autoconf-archive automake make pkgconfig mbedtls-devel glib2-devel jsoncpp-devel libuuid-devel libcap-ng-devel selinux-policy-devel``

#### Red Hat Enterprise Linux / CentOS / Scientific Linux
  First install the ``epel-release`` repository if that is not yet installed.  Then you can run:

  ``# yum install gcc-c++ git autoconf autoconf-archive automake make pkgconfig mbedtls-devel glib2-devel jsoncpp-devel libuuid-devel lz4-devel libcap-ng-devel selinux-policy-devel``


### Preparations building from git
- Clone this git repository: ``git clone git://github.com/OpenVPN/openvpn3-linux``
- Enter the ``openvpn3-linux`` directory: ``cd openvpn3-linux``
- Run: ``./bootstrap.sh``

Completing these steps will provide you with a `./configure` script.


### Adding the `openvpn` user and group accounts
The default configuration for the services assumes a service account
`openvpn` to be present. If it does not exist you should add one, e.g. by:

    # groupadd -r openvpn
    # useradd -r -s /sbin/nologin -g openvpn openvpn

### Building OpenVPN 3 Linux client
If you already have a `./configure` script or have retrieved an
`openvpn3-linux-*.tar.xz` tarball generated by `make dist`, the following steps
will build the client.

- Run: ``./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var``
- Run: ``make``
- Run: ``make install``

You might need to also reload D-Bus configuration to make D-Bus aware of
the newly installed service.  On most system this happens automatically,
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


#### Auto-completion helper for bash/zsh
The `openvpn3` front-end provides an interface for bash-completion to
retrieve valid sub-commands, options and arguments - including valid
D-Bus paths.  To enable this feature, copy
`src/shell/bash_completion-openvpn3` to either `/etc/bash_completion.d`
or `/usr/share/bash-completion/completions` (depending on the Linux
distribution).


#### SELinux

The `openvpn3-service-netcfg` service depends on being able to pass a file
descriptor to the tun device it has created on behalf of the
`openvpn3-service-client` service (where each of these processes represents
a single VPN session).  This is done via D-Bus.  But on systems with
SELinux, the D-Bus daemon is not allowed to pass file descriptors related
to `/dev/net/tun`.

The openvpn3-linux project ships an SELinux policy module, which will be
installed in `/etc/openvpn3/selinux` **if** the `./configure` script can
locate the SELinux policy development files.  On RHEL/Fedora the development
files are located under `/usr/share/selinux/devel` and provided by the
`selinux-policy-devel` package.

If the `selinux-policy-devel` package has been detected by `./configure`,
running `make install` will install the `openvpn3.pp` policy package,
typically in `/etc/openvpn3/selinux`.

This policy package adds a SELinux boolean, `dbus_access_tuntap_device`,
which grants processes, such as `dbus-daemon` running under the
`system_dbusd_t` security context access to files labelled as
`tun_tap_device_t`; which matches the label of `/dev/net/tun`.

To install and activate this SELinux security module, as root run:

         # semodule -i /etc/openvpn3/selinux/openvpn3.pp
         # semanage boolean --m --on dbus_access_tuntap_device

On Red Hat Enterprise Linux and Fedora, the `openvpn3-service-netcfg` will
stop running and the OpenVPN 3 Linux client will non-functional if this has
not been done.  The source code of the policy package can be found in
[`src/selinux/openvpn3.te`](src/selinux/openvpn3.te).

For users installing the pre-built RPM binaries, this is handled by the RPM
scriptlet during package install.


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
to tweak) via `openvpn3 log-service`.  The most important feature here is
probably to modify the log level.


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
to be accessible only by the openpn user - and even that users access is
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
  Please reach out on FreeNode @ #openvpn for help and discussing issues
  you encounter, or subscribe to and ask on the
  openvpn-users@lists.sourceforge.net mailing list.

* Packagers
  We are beginning to targeting packaging in Linux distributions.  The
  Fedora Copr repository is one which is currently available.  We are
  looking for people willing to package this in other Linux distributions
  as well.



DISCLAIMER
----------

The OpenVPN 3 Linux project is now considered BETA quality.   It is fully
functional and quite stable when running.

The OpenVPN 3 Core library is also used by the OpenVPN Connect and
PrivateTunnel clients in addition to the OpenVPN for Android client (need to
explicitly enable the OpenVPN 3 backend), so the pure VPN tunnel implementation
should be good to use.  However, this Linux implementation with the D-Bus
integration is fairly new.
