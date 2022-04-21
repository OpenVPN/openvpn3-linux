========
openvpn2
========

--------------------------------------
OpenVPN 2 front-end to OpenVPN 3 Linux
--------------------------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn2`` [ OPTIONS ]
| ``openvpn2`` ``-h`` | ``--help``


DESCRIPTION
===========
The **openvpn2** front-end to OpenVPN 3 Linux attempts to provide a similar
command line interface as the classic **OpenVPN 2.x** releases.  Since
**OpenVPN 3** does not support all the options available in **OpenVPN 2.x**,
some options is not available or will be ignored.

In addition the OpenVPN 3 Linux project provides only a client at the moment,
so all server side related options are not available.


OPTIONS
=======

-h, --help            show this help message and exit

--auth ALG            Authenticate packets with HMAC using message digest
                      algorithm alg (default=SHA1)

--auth-retry MODE
                      Defines how authentication failures should be handled.
                      Valid modes are:

                      :code:`none`
                            Disconnect on authentication failures (default)

                      :code:`nointeract`
                            Reuse already collected credentials

                      :code:`interact`
                            Ask for new credentials from the user

                      Currently, the OpenVPN 3 Linux client implementation
                      ignores this option.

--auth-user-pass      Authenticate with server using username/password

--ca FILE             Certificate authority file in .pem format containing
                      root certificate

--cd DIR              Change working directory to the given directory

--cert FILE           Certificate authority file in .pem format by a
                      Certificate Authority in ``--ca`` file

--cipher ALG          Encrypt packets with cipher algorithm alg (default=BF-
                      CBC)

--client              Configures client configuration mode (mandatory)

--comp-lzo <MODE>     Use LZO compression

--compress <ALG>      Compress using algorithm ALG

--config FILE         Read configuration options from file

--daemon              Run the VPN tunnel in the background

--dev DEV-NAME        Virtual interface name to use for VPN tunnel.
                      Defaults to :code:`tun`.  Usually ignored, as the
                      interface name is created on-the-fly in OpenVPN 3.

--dev-type DEV-TYPE   Defines the virtual interface type.  Only
                      :code:`tun` is supported, which is the default.

--dhcp-option OPTION  Set DHCP options which can be picked up by the OS
                      configuring DNS, etc.

--extra-certs FILE    Specify a file containing one or more PEM certs
                      (concatenated together) that complete the local
                      certificate chain.

--float               Allow remote to change its IP address/port.

--hand-window SEC     Handshake window.  The TLS-based key exchange must
                      finalize within SEC seconds handshake initiation by
                      any peer.  (Default 60 seconds)

--http-proxy ARGS
                      Connect to a remote host via a specified HTTP proxy.
                      This option takes 2 mandatory options, SERVER and PORT,
                      which defines the HTTP proxy and port to use.

                      Optional arguments are AUTH-FLAG which can be *auto-nct*
                      which enables clear-text passwords to be used.

                      OpenVPN 2.x also adds an optional AUTH-METHOD flag
                      as the last argument, this is auto-detected in
                      OpenVPN 3.

--http-proxy-user-pass FILE
                      Fetch HTTP proxy credentials from FILE

--ifconfig ARGS
                      Configures the TUN/TAP device for IPv4.  This option
                      takes two mandatory arguments, the IPv4 address to use
                      and the netmask for the network.

--ifconfig-ipv6 ARGS
                      Configures the TUN/TAP device for IPv6.  This option
                      takes one mandatory argument, the IPv6 address including
                      its PREFIX.  An optional REMOTE_ENDPOINT can be given
                      at the end.

--inactive ARGS
                      This option takes one mandatory argument, SECONDS, which
                      defines how many seconds the tunnel can idle before
                      disconnecting.  An optional BYTES argument can be added
                      which also takes the number of bytes passed over the
                      tunnel within SECONDS of inactivity.  The traffic must
                      be higher than this minimum BYTES to keep the tunnel
                      alive.

--keepalive ARGS
                      Instructs the client to ping the server over the
                      OpenVPN Control Channel every PING_SECONDS.  An optional
                      argument defines, RECONNECT_SECONDS how long it should go 
                      before the client should attempt to reconnect if there
                      is no response from the server.

--key FILE
                      Local private key in .pem format

--key-direction DIR
                      Set key direction for static keys.
                      Valid values: :code:`0`, :code:`1`

--local HOST
                      Local host name or IP address to to bind against on
                      local side

--lport PORT          TCP/UDP port number for local bind (default :code:`1194`)

--mode MODE           Operational mode.  Only ``client`` is accepted.

--mssfix BYTES        Set upper bound on TCP MSS (Default tun-mtu size)

--ns-cert-type TYPE   (DEPRECATED) Require that peer certificate is signed
                      with an explicit nsCertType designation.  Migrate to
                      ``--remote-cert-tls`` as soon as possible.  Valid
                      values: :code:`client`, :code:`server`

--persist-tun         Keep tun/tap device open across connection restarts

--ping SECS           Ping remote once per SECS seconds

--ping-restart SECS   Restart if n seconds pass without reception of remote
                      ping

--pkcs12 FILE         PKCS#12 file containing local private key, local
                      certificate and optionally the root CA certificate

--port PORT           TCP/UDP port number for both local and remote.

--profile-override OVERRIDE
                      OpenVPN 3 specific feature, allowing to set some local
                      overrides or disable some functionality.  This option
                      takes two arguments, an *OVERRIDE-KEY* and an
                      *OVERRIDE-VALUE*.  Valid keys and values are:

                      :code:`server-override`
                          A server host name

                      :code:`port-override`
                          A port number

                      :code:`proto-override`
                          Override connection protocol.
                          Valid values: :code:`tcp` or :code:`udp`

                      :code:`ipv6`
                          Enable or disable IPv6 inside the tunnel.
                          Valid values: :code:`yes`, :code:`no` or :code:`default`
                      :code:`enable-legacy-algorithms`
                          Enables non-AEAD ciphers supported by the TLS library.
                          See ``openvpn3-config-manage``\(1) for details.

                      :code:`dns-setup-disabled`
                          Disable configuring pushed DNS settings.
                          Valid values: :code:`true` or :code:`false`

                      :code:`dns-sync-lookup`
                          Do synchronous DNS lookup instead of the default,
                          asynchronous.
                          Valid values: :code:`true` or :code:`false`

                      :code:`auth-fail-retry`
                          Try to reconnect even if the server rejected the
                          connection due to authentication failure.
                          Valid values: :code:`true` or :code:`false`

                      :code:`proxy-host`
                          Proxy server host name for the VPN tunnel

                      :code:`proxy-port`
                        Proxy server port number

                      :code:`proxy-username`
                        Username used for proxy authentication

                      :code:`proxy-password`
                        Password used for proxy authentication

                      :code:`proxy-auth-cleartext`
                        Valid values: :code:`true` or :code:`false`

                      These overrides are described further in
                      ``openvpn3-config-manage``\(1)

--proto PROTO         Use protocol PROTO for communicating with peer.
                      Valid values: :code:`udp`, :code:`tcp`

--push-peer-info      Push client info to server

--redirect-gateway FLAGS
                      Automatically execute routing commands to redirect all
                      outgoing IP traffic through the VPN.  Valid flags:
                      :code:`autolocal`, :code:`def1`, :code:`bypass-dhcp`,
                      :code:`bypass-dns`, :code:`block-local`, :code:`ipv4`,
                      :code:`!ipv4`, :code:`ipv6`, :code:`!ipv6`

--redirect-private FLAGS
                      Like ``--redirect-gateway``, but omit actually changing
                      default gateway.  Valid flags: :code:`autolocal`,
                      :code:`def1`, :code:`bypass-dhcp`, :code:`bypass-dns`,
                      :code:`block-local`, :code:`ipv4`, :code:`!ipv4`,
                      :code:`ipv6`, :code:`!ipv6`

--remote ARGS
                      Defines the remote server to connect to.  One
                      mandatory argument must be given, containing either
                      an *IP address* or an *hostname* to the server.  An
                      optional *PORT* number can be given
                      (default: :code:`1194`) and at the end the *PROTOCOL*
                      can be specified (default: ``udp``).  This option can
                      be given multiple times and the client will try all
                      remote entries until it is able to establish a
                      connection.  The order of arguments are: *HOST/IP*,
                      *PORT* and *PROTOCOL*

--remote-cert-eku OID
                      Require the peer certificate to be signed with
                      explicit extended key usage.  *OID* can be an object
                      identifier or OpenSSL string representation.

--remote-cert-ku ID
                      Require that the peer certificate was signed with
                      explicit key usage (*ID*).  More than one ID can be
                      provided.  Must be hexadecimal notation of integers

--remote-cert-tls TYPE
                      Require that peer certificate is signed with explicit
                      key usage and extended key usage based RFC3280 rules.
                      Valid values: :code:`client`, :code:`server`

--remote-random       If multiple ``--remote`` options specified, choose one
                      randomly

--reneg-sec SECS      Renegotiate data channel key after SECS seconds.
                      (Default: :code:`3600`)

--route ARGS
                      Add route to routing table after connection is
                      established.  Multiple routes can be specified.

                      This option takes one mandatory argument, IP-ADDRESS
                      to route over the VPN.  The two optional arguments
                      are NETMASK (default: :code:`255.255.255.255`) and the
                      gateway to use (defaults to use configured
                      ``--route-gateway`` or the VPN server IP address).

--route-gateway <GW|dhcp>
                      Specify a default gateway for use with ``--route``.
                      See ``openvpn``\(8) man page for dhcp mode

--route-ipv6 ARGS
                      Add IPv6 route to routing table after connection is
                      established.  Multiple routes can be specified.

                      This option takes one mandatory argument IP-RANGE/PREFIX.
                      An optional *GATEWAY* can be set, which overrides the
                      default server VPN IPv6 address and the second
                      argument which sets the route *METRIC* value.

--route-metric METRIC
                      Specify a default metric for use with ``--route``

--route-nopull        Do not configure routes pushed by remote server

--server-poll-timeout SECS
                      How long to wait for a response from a remote server
                      during connection setup (Default: *120* seconds)

--setenv ARGS
                      Set a custom environmental variable to pass to script.
                      This takes two mandatory arguments, variable NAME
                      and VALUE.

--static-challenge ARGS
                      Enable static challenge/response protocol.  This
                      takes one mandatory option, *MESSAGE*, which will
                      be presented to the user before the connection
                      attempt.  An optional argument, *ECHO*, indicates
                      if the user input should be echoed back to the
                      user during input entry.

--tcp-queue-limit NUM
                      Maximum number (*NUM*)of queued TCP output packets

--tls-auth ARGS
                      Enables an additional HMAC authentication on TLS
                      control channel.  This takes a mandatory argument,
                      *FILE*, which must be a shared secret between server
                      and client.  The optional *KEY-DIRECTION* argument
                      defines which sub-key pair in *FILE* to use for HMAC
                      signing and verification.
                      Valid values are :code:`0` or :code:`1`.

--tls-cert-profile PROFILE
                      Sets certificate profile which defines acceptable
                      crypto algorithms.  Valid profiles: legacy, preferred,
                      suiteb

--tls-client          Enable TLS and assume client role during TLS
                      handshake.  Implicitly added when using ``--client``

--tls-crypt FILE      Encrypts the TLS control channel with a shared secret
                      key (FILE).  This CANNOT be combined with ``--tls-auth``

--tls-timeout SECS    Packet retransmit timeout on TLS control channel if
                      no ACK from remote within n seconds.
                      (Default: *2* seconds)

--topology TYPE       Set tunnel topology type.  Default is :code:`net30`.
                      Recommended: :code:`subnet`, but this must match the
                      server setting.
                      Valid topologies: :code:`subnet`, :code:`net30`

--tran-window SECS    Transition window -- old data channel key can live
                      this many seconds after new after new key renegotiation
                      begins.  (Default: :code:`3600` secs)

--tun-mtu SIZE        Set TUN/TAP device MTU to SIZE and derive TCP/UDP from
                      it (default is 1500)

--verb LEVEL          Set log verbosity level.  Log levels are NOT compatible
                      with OpenVPN 2 ``--verb``

--verify-x509-name ARGS
                     Accept connections only with a host with a specific
                     X509 subject or CN match string.  This option takes
                     one mandatory argument, which is a MATCH string and
                     an optional match FLAG.

                     FLAG can be:

                     :code:`name`
                       Match against complete X.509 Common Name field


                     :code:`name-prefix`
                       The MATCH value must be match the beginning of the
                       X.509 Common Name field.  If the X.509 certificate
                       contains :code:`server-1.example.org`, it will be a
                       match if the MATCH value is :code:`server-`.  It will
                       not be a match if values like :code:`server-2` or
                       :code:`.example.org` is used.


                     :code:`subject`
                       The MATCH value must be the full and complete
                       X.509 Subject field.  This is the default behaviour.

TECH-PREVIEW OPTIONS
====================
These options are only present for testing new bleeding edge features. There are
no guarantees they will work, will not change or will not change behaviour in
the future.  These options are *NOT* ready for production environments.

--enable-dco | --disable-dco
                     Enable or disabled the Data Channel Offload (DCO) kernel
                     acceleration module support.  The default is disabled, but
                     this option is present for compatibility with OpenVPN 2.6.
                     The :code:`--enable-dco` option is OpenVPN 3 Linux specific
                     as this project does currently not automatically detect and
                     enable the DCO capability of the host.


IGNORED OPTIONS
===============
The options in this list will be silently ignored.  Some of these options
have not yet been implemented in the OpenVPN 3 Core library and others
are not relevant any more.  But none of these options will break any
existing configurations.

--auth-nocache        Do not cache --askpass or --auth-user-pass in virtual
                      memory.  Not applicable with OpenVPN 3 due to different
                      credentials storage model.

--chroot DIR          Chroot to this directory after initialization.  Not
                      applicable with OpenVPN 3, which uses a different
                      execution model.

--data-ciphers CIPHERLIST
                      OpenVPN 2.5 introduced this option has a replacement
                      to ``--ncp-ciphers``.  This is primarily intended to
                      be used when migrating away from the prior default
                      BF-CBC cipher.  With Negotiable Cipher Parameters
                      (NCP), this should not be needed in the future.
                      OpenVPN 3 also has a different way of handling this
                      situation and is believed to not have the same
                      connectivity issues as OpenVPN 2.4 and newer 2.x
                      releases could have against older OpenVPN 2.x
                      servers.

--data-ciphers-fallback ALG
                      This is tightly coupled to ``--data-ciphers`` and is
                      also not used nor supported by OpenVPN 3.

--dev-node NODE       OpenVPN 2.x will use /dev/net/tun, /dev/tun, /dev/tap,
                      etc by default when creating the tun/tap interface.  This
                      is handled differently in OpenVPN 3 Linux and is not
                      configurable by front-ends like ``openvpn2`` or
                      ``openvpn3``, since the virtual network interface creation
                      is handled by the ``openvpn3-service-netcfg``\(8) service.

--down                Run a script after the tunnel has been torn down.
                      Running scripts via OpenVPN 3 is not supported, and
                      using this option will display a warning.  See the
                      NOTES section below for details.

--down-pre            This is related to when the ``--down`` script is being
                      run during the disconnection.  See the NOTES section
                      below regarding script execution in OpenVPN 3.

--explicit-exit-notify <ATTEMPTS>
                        On exit/restart, send exit signal to remote end.
                        Automatically configured with OpenVPN 3

--group GROUP         Run OpenVPN with GROUP group credentials.  Not needed
                      with OpenVPN 3 which uses a different privilege
                      separation approach

--mute-replay-warnings
                      OpenVPN 2.5 and older can hide warnings related to
                      replayed packets.  Packet replays are not reported
                      in the same way in OpenVPN 3 Core library, so this
                      option makes no behavioural change.

--ncp-ciphers CIPHERLIST
                      OpenVPN 2.4 option renamed to ``--data-ciphers`` in
                      OpenVPN 2.5.  Ignored in OpenVPN 3.

--nice LEVEL          Change process priority.  Not supported in OpenVPN 3

--nobind              Do not bind to local address and port.  This is default
                      behaviour in OpenVPN 3
--persist-key         Do not re-read key files across connection restarts.
                      Not needed.  OpenVPN 3 keeps keys as embedded file
                      elements in the configuration

--rcvbuf SIZE         Set the TCP/UDP receive buffer size.  Not supported in
                      OpenVPN 3

--resolv-retry SECS   If hostname resolve fails for ``--remote``, retry
                      resolve for n seconds before failing.  Not supported
                      by OpenVPN 3.

--script-security LEVEL
                      This option is ignored, as OpenVPN 3 itself does not
                      execute any external scripts.

--sndbuf SIZE         Set the TCP/UDP send buffer size.  Not supported in
                      OpenVPN 3.

--socket-flags FLAGS
                      Applies flags to the transport socket.  Not supported
                      in OpenVPN 3.

--up                  Run a script after the tunnel has been established.
                      Running scripts via OpenVPN 3 is not supported, and
                      using this option will display a warning.  See the
                      NOTES section below for details.

--user USER           Run OpenVPN with USER user credentials.  Not needed
                      with OpenVPN 3 which uses a different privilege
                      separation approach


NOTES
=====

SCRIPT EXECUTION
----------------

OpenVPN 3 does not implement any support for running external scripts or program
during its life cycle.  This is by design.  Running scripts is a security risk,
and needs to be handled carefully.  In classic OpenVPN 2.x setups, scripts are
run with the same privileges as the ``openvpn``\(8) process.  If the process is
started as root, the script may be run as root.  Which is why the
``--script-security`` option is available and by default disabling running most
external programs.

This does not mean it is impossible to trigger programs to perform operations
when certain OpenVPN events occur.  OpenVPN 3 Linux is using D-Bus actively and
it issues several signals as the state changes.  It also means you can write
your own front-end doing its own calls how you prefer while starting and
managing the VPN session at the same time.  This allows a much better
flexibility and allows to adopt VPN session management into the execution flow
which is needed.  And the implementation can do its own security assessments on
how it will tackle these scenarios.

There are at least three ways how to adopt to the OpenVPN 3 model:

1.  Watching D-Bus StateChange signals for your own sessions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When a session is started, it is possible to subscribe to signals issued by the
VPN client process over D-Bus.  It is only possible to subscribe to signals
related to the session owner's own sessions.  These signals are sent by the
Session Manager (``net.openvpn.v3.sessions``,
``openvpn3-service-sessionmgr``\(8))

Example:
::

    $ dbus-monitor --system --monitor sender=net.openvpn.v3.sessions,interface=net.openvpn.v3.sessions,member=StatusChange


2.  Manage the life cycle of VPN sessions on your own
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This means wrapping the starting of VPN sessions on your own.  Either you wrap
``openvpn3 session-start`` or ``openvpn2`` calls in your own scripts, or you can
connect directly to the Configuration Manager (``net.openvpn.v3.configuration``,
``openvpn3-service-configmgr``\(8)) and Session Manager
(``net.openvpn.v3.sessions``, ``openvpn3-service-sessionmgr``\(8)) to import
configuration profiles and start/stop VPN sessions as needed, as well as
subscribing to D-Bus signals as well to handle various the states a VPN session
will go through.  This is fairly simple to do using the already available
openvpn3 Python module.  Example code can be found in the
`OpenVPN 3 Linux source tree`_ [#srctree]_
or by studying the source code of ``openvpn2``\(1) and ``openvpn3-autoload``\(8),
which both are Python scripts.

Configurations and sessions managed via D-Bus by your own scripts can still be
further managed by the ``openvpn3``\(1) command line interface.


3.  Subscribing to NetworkChange signals from `net.openvpn.v3.netcfg`_ [#netcfgsrv]_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is also a scripting possibility, which is more useful for system wide
script triggering.  A program or script can subscribe to specific network change
events caused by OpenVPN sessions.  These signals contains information about
virtual network interfaces which has been created or removed, IP addresses added
or removed from devices, routing configuration as well as DNS resolver changes.

For an example how to do this, see the `example script`_ [#examplescript]_
in the OpenVPN 3 Linux source directory.

**Please note** that, by default, this script must be run as ``root`` or the
``openvpn`` user on the system.  It is possible to allow other users or groups
this privilege, by extending the D-Bus policy for the ``net.openvpn.v3.netcfg``
service.  But granting this privilege too widely may result in unwanted
information leakage related to VPN interface configurations.


SEE ALSO
========

``openvpn``\(8)
``openvpn3``\(1)
``openvpn3-config-manage``\(1)

.. [#srctree] https://github.com/OpenVPN/openvpn3-linux/tree/master/src/tests/python
.. [#netcfgsrv] https://github.com/OpenVPN/openvpn3-linux/blob/master/docs/dbus/dbus-service-net.openvpn.v3.netcfg.md
.. [#examplescript] https://github.com/OpenVPN/openvpn3-linux/blob/master/src/tests/python/netcfg-netchg-subscription
.. _OpenVPN 3 Linux source tree: https://github.com/OpenVPN/openvpn3-linux/tree/master/src/tests/python
.. _net.openvpn.v3.netcfg: https://github.com/OpenVPN/openvpn3-linux/blob/master/docs/dbus/dbus-service-net.openvpn.v3.netcfg.md
.. _example script: https://github.com/OpenVPN/openvpn3-linux/blob/master/src/tests/python/netcfg-netchg-subscription
