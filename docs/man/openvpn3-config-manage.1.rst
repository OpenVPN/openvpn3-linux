======================
openvpn3-config-manage
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-manage`` ``-o DBUS-PATH`` | ``--path DBUS-PATH`` | ``--config CONFIG-NAME`` [OPTIONS]
| ``openvpn3 config-manage`` ``-h`` | ``--help``


DESCRIPTION
===========
This will delete an imported configuration profile from the configuration
manager.

OPTIONS
=======

-h, --help              Print  usage and help details to the terminal

-o DBUS-PATH, --path DBUS-PATH
                        D-Bus configuration path to the
                        configuration to delete.  This can be found in
                        ``openvpn3 configs-list``.

--config-path DBUS-PATH
                        Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                        Can be used instead of ``--path`` where the
                        configuration profile name is given instead.  Available
                        configuration names can be found via
                        ``openvpn3 configs-list``.

-r NEW-CONFIG-NAME, --rename NEW-CONFIG-NAME
                        Renames the configuration profile

-s, --show
                        Show the current profile settings

--dco BOOL
                        Enable kernel based Data Channel Offload.  This moves
                        the tunnelled network traffic to be handled inside the
                        kernel.  This improves the processing of the network
                        traffic and moves the encryption, decryption and packet
                        authentication for the tunnelled network traffic to be
                        handled inside the kernel instead of begin passed via
                        the OpenVPN client process in user space.

                        This option is only available if openvpn3-linux has been
                        built with this support.

                        *WARNING:*
                            This is currently a **tech preview** feature
                            and is **not** ready for production environments.
                            It also requires the `ovpn-dco` kernel module to be
                            installed to work and at least a Linux 5.4 kernel.

--server-override HOST
                        Override the remote server hostname/IP address to
                        connect against.

--port-override PORT
                        Override the remote server port to connect against.
                        Valid values: :code:`1` to :code:`65535`.

--proto-override PROTO
                        Override the connection protocol.  Valid values are
                        :code:`tcp` and :code:`udp`.

--ipv6 ARG
                        Sets the IPv6 connect policy for the client.  Valid
                        values are :code:`yes`, :code:`no` and :code:`default`

--persist-tun BOOL
                        Overrides the ``--persist-tun`` argument in the
                        configuration profile.  If set to true, the tun
                        adapter will persist during the reconnect.  If false,
                        the tun adapter will be torn down before reconnects.
                        Valid values are: :code:`true`, :code:`false`

--log-level LEVEL
                        Overrides the default log level.  The default log level
                        is ``3`` if the configuration file does not contain a
                        ``--verb`` option.  This override will take place over
                        any other log verbosity settings.  Valid values are
                        between ``1`` and ``6``.

--dns-fallback-google BOOL
                        If set to true, the DNS resolver settings will include
                        Google DNS servers.  Valid values are: :code:`true`,
                        :code:`false`


--dns-scope SCOPE
                        Defines the DNS query scope.  This is currently only
                        supported when enabling the `systemd-resolved`\(8)
                        resolver support in `openvpn3-service-netcfg`\(8).
                        Supported values are:

                        :code:`global`:  (default)
                          The VPN service provided DNS server(s) will be used
                          for all types of DNS queries.

                        :code:`tunnel`:
                          The VPN service provided DNS server(s) will only be
                          used for queries for DNS domains pushed by the
                          VPN service.

                          **NOTE**
                            The DNS domains pushed by the VPN service may be
                            queried by DNS servers with `systemd-resolved`\(8)
                            service if their respective interfaces are
                            configured to do global DNS queries.  But other
                            non-listed DNS domains will not be sent to this
                            VPN service provider's DNS server.

--dns-setup-disabled BOOL
                        If set to true, DNS settings will not be configured
                        on the system.  Valid values are: :code:`true`,
                        :code:`false`


--dns-sync-lookup BOOL
                        If set to true, DNS lookups will happen synchronously.
                        Valid values are: :code:`true`, :code:`false`

--auth-fail-retry BOOL
                        If set to true, the client will try to reconnect instead
                        of disconnecting if authentication fails.  Valid values
                        are: :code:`true`, :code:`false`

--allow-compression ARG
                        This controls whether the client wants to allow
                        compression on traffic between the client to the server.
                        Valid argument values:

                        :code:`no`:
                          Do not compress at all

                        :code:`asym`:
                          Only allow server to send compressed data

                        :code:`yes`:
                          Both client and server can use compression

--enable-legacy-algorithms BOOL
                        By default, OpenVPN 3 Linux only expects to work with
                        servers capable of doing AEAD ciphers on the data
                        channel, such as AES-GCM or ChaCha20-Poly1305 (if
                        supported by the TLS library).  To connect to legacy
                        servers not capable of AEAD ciphers on the data channel,
                        it might help to enable legacy cipher algorithms.

--tls-version-min ARG
                        Sets the minimum TLS version for the control channel.
                        For this to be functional, the SSL/TLS library in use
                        needs to support this restriction on both server and
                        client.  Valid argument values are:

                        :code:`tls_1_0`:
                          Enforce minimum TLSv1.0

                        :code:`tls_1_1`:
                          Enforce minimum TLSv1.1

                        :code:`tls_1_2`:
                          Enforce minimum TLSv1.2

                        :code:`tls_1_3`:
                          Enforce minimum TLSv1.3.  This is currently only
                          supported by OpenSSL 1.1.1.


--tls-cert-profile ARG
                        This sets the acceptable certificate and key parameters.
                        Valid argument values are:

                        :code:`legacy`:
                          Allows minimum 1024 bits RSA keys with certificates
                          signed with SHA1.

                        :code:`preferred`:
                          Allows minimum 2048 bits RSA keys with certificates
                          signed with SHA256 or higher. (default)

                        :code:`suiteb`:
                          This follows the NSA Suite-B specification.


--proxy-host PROXY-SERVER
                        HTTP proxy to establish the VPN connection via.

--proxy-port PROXY-PORT
                        Port where the HTTP proxy is available.

--proxy-username PROXY-USER
                        Username to use for the HTTP proxy connection

--proxy-password PROXY-PASSWORD
                        Password to use for the HTTP proxy connection

--proxy-auth-cleartext BOOL
                        Allow HTTP proxy authentication to happen in clear-text.
                        Valid values are: :code:`true`, :code:`false`

--unset-override OVERRIDE
                        This removes an override setting from the configuration
                        profile.  The ``OVERRIDE`` value is the setting
                        arguments enlisted here but without the leading ``--``.
                        For example, if ``--tls-cert-profile suiteb`` was set,
                        it can be unset with
                        ``--unset-override tls-cert-profile``.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-acl``\(1)
``openvpn3-config-import``\(1)
``openvpn3-configs-list``\(1)
``openvpn3-config-remove``\(1)
