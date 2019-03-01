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

--server-override HOST
                        Override the remote server hostname/IP address to
                        connect against.

--port-override PORT
                        Override the remote server port to connect against.
                        Valid values: 1-65535.

--proto-override tcp|udp
                        Override the connection protocol.  Valid values are
                        ``tcp`` and ``udp``.

--ipv6 yes|no|default
                        Sets the IPv6 connect policy for the client.

--persist-tun true|false
                        Overrides the --persist-tun argument in the
                        configuration profile.  If set to true, the tun
                        adapter will persist during the reconnect.  If false,
                        the tun adapter will be torn down before reconnects.

--dns-fallback-google true|false
                        If set to true, the DNS resolver settings will include
                        Google DNS servers.

--dns-setup-disabled true|false
                        If set to true, DNS settings will not be configured
                        on the system.

--dns-sync-lookup true|false
                        If set to true, DNS lookups will happen synchronously.

--auth-fail-retry true|false
                        If set to true, the client will try to reconnect instead
                        of disconnecting if authentication fails.

--no-client-cert true|false
                        If set to true, the client does not provide any client
                        certificates to the server.

--allow-compression no|asym|yes
                        This controls whether the client wants to allow
                        compression on traffic between the client to the server.

                        * *no*:
                          Do not compress at all

                        * *asym*:
                          Only allow server to send compressed data

                        * *yes*:
                          Both client and server can use compression

--force-cipher-aes-cbc true|false
                        Override ``--cipher`` and disable cipher negotiation
                        and force AES-CBC cipher to be used.

--tls-version-min tls_1_0|tls_1_1|tls_1_2|tls_1_3
                        Sets the minimum TLS version for the control channel.
                        For this to be functional, the SSL/TLS library in use
                        needs to support this restriction on both server and
                        client.

                        * *tls_1_0*:
                          Enforce minimum TLSv1.0

                        * *tls_1_1*:
                          Enforce minimum TLSv1.1

                        * *tls_1_2*:
                          Enforce minimum TLSv1.2

                        * *tls_1_3*:
                          Enforce minimum TLSv1.3.  This is currently only
                          supported by OpenSSL 1.1.1.


--tls-cert-profile legacy|preferred|suiteb
                        This sets the acceptable certificate and key parameters.

                        * *legacy*:
                          Allows minimum 1024 bits RSA keys with certificates
                          signed with SHA1.

                        * *preferred*:
                          Allows minimum 2048 bits RSA keys with certificates
                          signed with SHA256 or higher. (default)

                        * *suiteb*:
                          This follows the NSA Suite-B specification.


--proxy-host PROXY-SERVER
                        HTTP proxy to establish the VPN connection via.

--proxy-port PROXY-PORT
                        Port where the HTTP proxy is available.

--proxy-username PROXY-USER
                        Username to use for the HTTP proxy connection

--proxy-password PROXY-PASSWORD
                        Password to use for the HTTP proxy connection

--proxy-auth-cleartext true|false
                        Allow HTTP proxy authentication to happen in clear-text.

--unset-override OVERRIDE
                        This removes an override setting from the configuration
                        profile.  The ``OVERRIDE`` value is the the setting
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
