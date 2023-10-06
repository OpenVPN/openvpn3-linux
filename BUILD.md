How to build openvpn3-linux locally
-----------------------------------

The primary Linux distributions targeted and regularly tested are:

  - CentOS 7 and 8
  - Debian 11 and newer
  - Fedora 37 and newer
  - Red Hat Enterprise Linux (RHEL) 7 and newer
  - Scientific Linux 7
  - Ubuntu 20.04 and newer

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

* GLib2 2.56 or newer

  http://www.gtk.org
  This dependency is due to the GDBus library, which is the D-Bus
  implementation being used.

* jsoncpp 0.10.5 or newer

  https://github.com/open-source-parsers/jsoncpp

* libcap-ng 0.7.5 or newer

  http://people.redhat.com/sgrubb/libcap-ng

* liblz4 1.8.4 or newer

  https://lz4.github.io/lz4

* libuuid 2.23.2 or newer

  https://en.wikipedia.org/wiki/Util-linux

* polkit 0.112 or newer

  http://www.freedesktop.org/wiki/Software/polkit

  Only needed when using the systemd-resolved integration.  On Ubuntu this
  package is called policykit-1.

* tinyxml2 2.1.0 or newer

  https://github.com/leethomason/tinyxml2

* (optional) libnl3 3.2.29 or newer

  http://www.infradead.org/~tgr/libnl/

  Only needed when building with DCO support

* (optional) protobuf 2.4.0 or newer

  http://code.google.com/p/protobuf/

  Only needed when building with DCO support

* (optional) Python 3.6 or newer

  If Python 3.6 or newer is found, the openvpn2, openvpn3-autoload utilities
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

- Building with mbed TLS (alternative):

      # apt-get install libmbedtls-dev

- Generic build requirements:

      # apt-get install build-essential git pkg-config autoconf         \
                        autoconf-archive libglib2.0-dev libjsoncpp-dev  \
                        uuid-dev liblz4-dev libcap-ng-dev libxml2-utils \
                        python3-minimal python3-dbus python3-docutils   \
                        python3-jinja2 libxml2-utils libtinyxml2-dev    \
                        policykit-1 libsystemd-dev python3-systemd

- Dependencies to build with DCO support:

      # apt-get install libnl-3-dev libnl-genl-3-dev protobuf-compiler libprotobuf-dev


#### Fedora:

- Building with OpenSSL (recommended):

      # yum install openssl-devel

- Building with mbed TLS (alternative):

      # yum install mbedtls-devel

- Generic build requirements:

      # yum install gcc-c++ git autoconf autoconf-archive automake make \
                    pkgconfig glib2-devel jsoncpp-devel libuuid-devel   \
                    libcap-ng-devel selinux-policy-devel lz4-devel      \
                    zlib-devel libxml2 tinyxml2-devel python3-dbus      \
                    python3-gobject python3-pyOpenSSL python3-jinja2    \
                    python3-docutils bzip2 polkit systemd-devel         \
                    python3-systemd

- Dependencies to build with DCO support:

      # yum install libnl3-devel protobuf-compiler protobuf protobuf-devel


#### Red Hat Enterprise Linux / CentOS / Scientific Linux
  First install the ``epel-release`` repository if that is not yet
  installed.

##### NOTES: RHEL 8 / CentOS 8 - differences from RHEL 7 / CentOS 7
  For Red Hat Enterprise Linux 8 and CentOS 8, use the package lists used
  in Fedora.  In addition the CodeReady (RHEL) / PowerTools (CentOS)
  repository needs to be enabled when building with Data Channel Offload(DCO)
  support.

  For CentOS 8 run these commands:

     # yum -y install yum-utils
     # yum-config-manager --set-enabled powertools

##### Required packages

- Building with OpenSSL (recommended)

      # yum install openssl-devel

- Building with mbed TLS (alternative):

      # yum install mbedtls-devel

- Generic build requirements (only RHEL 7):

      # yum install gcc-c++ git autoconf autoconf-archive automake make \
                    pkgconfig glib2-devel jsoncpp-devel libuuid-devel   \
                    lz4-devel libcap-ng-devel selinux-policy-devel      \
                    zlib-devel libxml2 python-docutils python36         \
                    python36-dbus python36-gobject python36-pyOpenSSL   \
                    polkit systemd-devel tinyxml2 tinyxml2-devel

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
- Run (as `root`): ``make install``
- Run (as `root`): ``openvpn3-admin init-config --write-configs``

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


#### Auto-completion helper for bash/zsh

If you want to also install the bash-completion scripts for the
``openvpn2`` and ``openvpn3`` commands, add ``--enable-bash-completion``
to the ``./configure`` command.
