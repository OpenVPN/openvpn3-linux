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

--config-show
                Shows the contents of the configuration file used by
                ``openvpn3-service-netcfg``\(8).

--config-set
                Modifies a configuration setting in the configuration file.
                Changing the configuration file will require the
                ``openvpn3-service-netcfg``\(8) process to be restarted before
                they become active.  This option requires two arguments:
                *CONFIG-NAME* and *CONFIG-VALUE*.  Valid *CONFIG-NAMEs* are:

                :code:`log-level`
                        Defines the default log-level to use when
                        ``openvpn3-service-netcfg``\(8) starts.  Valid values
                        are integers between :code:`0` and :code:`6`.

                :code:`log-file`
                        Sets the log destination for log events.  See the
                        ``--log-file`` option in the man page to
                        ``openvpn3-service-netcfg``\(8) for details.

                :code:`idle-exit`
                        Sets the automatic idle shut down time-out.  See the
                        ``--idle-exit`` option in the man page to
                        ``openvpn3-service-netcfg``\(8) for details.

                :code:`resolv-conf`
                        Defines which :code:`resolv.conf` file should be used
                        for DNS resolver configuration.  See the
                        ``--resolv-conf`` option in the man page to
                        ``openvpn3-service-netcfg``\(8) for details.

                :code:`systemd-resolved`
                        Configures the DNS resolver configuration to use
                        ``systemd-resolved``\(8).  See the
                        ``--systemd-resolved`` option in the man page to
                        ``openvpn3-service-netcfg``\(8) for details.  To
                        activate this feature, the *COFIG-VALUE* must be
                        :code:`1` or :code:`yes`.

                :code:`redirect-method`
                        Configures the host route/redirect method to use
                        to access the VPN server when ``--redirect-gateway``
                        is used.  See the ``--redirect-method`` option in the
                        man page to ``openvpn3-service-netcfg``\(8) for
                        details.  Valid *CONFIG-VALUEs* are:
                        :code:`host-route`, :code:`bind-device` or
                        :code:`none`.

                :code:`set-somark`
                        Configures the Netfilter SO_MARK value to use for
                        OpenVPN traffic.  See the ``--redirect-method``
                        option in the man page to
                        ``openvpn3-service-netcfg``\(8) for details.

--config-unset
                Similar to ``--config-set`` but removes a setting from the
                configuration file.

--list-subscribers
                Lists all services which has been granted a subscription for
                NetworkChange signals and which change types they have
                requested a notification for.  These signals are sent each time
                a virtual network adapter is created, modified or destroyed,
                including DNS changes.

--unsubscribe DBUS-UNIQUE-NAME
                This can forcefully unsubscribe a service from receiving
                NetworkChange signals.  The argument must be the unique
                D-Bus name (typically :code:`:1.xxxx` where **x** denotes an
                integer).

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-service-netcfg``\(8)
