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

--auth-user-pass      Authenticate with server using username/password

--ca FILE             Certificate authority file in .pem format containing
                      root certificate

--cd DIR              Change working directory to the given directory

--cert FILE           Certificate authority file in .pem format by a
                      Certificate Authority in --ca file

--cipher ALG          Encrypt packets with cipher algorithm alg (default=BF-
                      CBC)

--client              Configures client configuration mode (mandatory)

--comp-lzo <MODE>     Use LZO compression

--compress <ALG>      Compress using algorithm ALG

--config FILE         Read configuration options from file

--daemon              Run the VPN tunnel in the background

--dev DEV-NAME        tun/tap device to use for VPN tunnel

--dev-type DEV-TYPE   Which device type are we using? tun or tap. Not needed
                      if --dev starts with tun or tap

--dhcp-option OPTION  Set DHCP options which can be picked up by the OS
                      configuring DNS, etc

--extra-certs FILE    Specify a file containing one or more PEM certs
                      (concatenated together) that complete the local
                      certificate chain.

--float               Allow remote to change its IP address/port

--hand-window SEC     Handshake window. The TLS-based key exchange must
                      finalize within SEC seconds handshake initiation by
                      any peer. (Default 60 seconds)

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

--key FILE            Local private key in .pem format

--key-direction DIR   Set key direction for static keys. Valid values: 0, 1

--local HOST          Local host name or IP address to to bind against on
                      local side

--lport PORT          TCP/UDP port number for local bind (default 1194)

--mode MODE           Operational mode. Only "client" is accepted

--mssfix BYTES        Set upper bound on TCP MSS (Default tun-mtu size)

--ns-cert-type TYPE   (DEPRECATED) Require that peer certificate is signed
                      with an explicit nsCertType designation. Migrate to
                      --remote-cert-tls ASAP. Valid values: client, server

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

                      * *server-override*:
                        A server host name

                      * *port-override*:
                        A port number

                      * *proto-override*:
                        *tcp* or *udp*

                      * *ipv6*:
                        *yes*, *no* or *default*

                      * *dns-setup-disabled*:
                        *true* or *false*

                      * *dns-sync-lookup*:
                        *true* or *false*

                      * *auth-fail-retry*:
                        *true* or *false*

                      * *proxy-host*:
                        Proxy server host name

                      * *proxy-port*:
                        Proxy server port number

                      * *proxy-username*:
                        Username used for proxy authentication

                      * *proxy-password*:
                        Password used for proxy authentication

                      * *proxy-auth-cleartext*:
                        *true* or *false*

                      These overrides are described further in
                      ``openvpn3-config-manage``\(1)

--proto PROTO         Use protocol PROTO for communicating with peer. Valid
                      values: udp, tcp

--push-peer-info      Push client info to server

--redirect-gateway FLAGS
                      Automatically execute routing commands to redirect all
                      outgoing IP traffic through the VPN. Valid flags:
                      autolocal, def1, bypass-dhcpbypass-dns, block-local,
                      ipv4, !ipv4, ipv6, !ipv6

--redirect-private FLAGS
                      Like --redirect-gateway, but omit actually changing
                      default gateway.Valid flags: autolocal, def1, bypass-
                      dhcpbypass-dns, block-local, ipv4, !ipv4, ipv6, !ipv6

--remote ARGS
                      Defines the remote server to connect to.  One mandatory
                      argument must be given, containing either an IP address
                      or an hostname to the server.  An optional PORT number
                      can be given (default: 1194) and at the very end the
                      PROTOCOL can be specified (default: udp).  This option
                      can be given multiple times and the client will try
                      all remote entries until it is able to establish
                      a connection.

--remote-cert-eku OID
                      Require the peer certificate to be signed with
                      explicit extended key usage. OID can be an object
                      identifier or OpenSSL string representation.

--remote-cert-ku ID
                      Require that the peer certificate was signed with
                      explicit key usage (ID). More than one ID can be
                      provided. Must be hexadecimal notation of integers

--remote-cert-tls TYPE
                      Require that peer certificate is signed with explicit
                      key usage and extended key usage based RFC3280 rules.
                      Valid values: client, server

--remote-random       If multiple --remote options specified, choose one
                      randomly

--reneg-sec SECS      Renegotiate data channel key after SECS seconds.
                      (Default 3600)

--route ARGS
                      Add route to routing table after connection is
                      established. Multiple routes can be specified.

                      This option takes one mandatory argument, IP-ADDRESS
                      to route over the VPN.  The two optional arguments
                      are NETMASK (default: 255.255.255.255) and the
                      gateway to use (defaults to use configured
                      --route-gateway or the VPN server IP address).


--route-gateway <GW|dhcp>
                      Specify a default gateway for use with --route. See
                      openvpn\(8) man page for dhcp mode

--route-ipv6 ARGS
                      Add IPv6 route to routing table after connection is
                      established. Multiple routes can be specified.

                      This option takes one mandatory argument IP-RANGE/PREFIX.
                      An optional GATEWAY can be set, which overrides the
                      default server VPN IPv6 address and the second
                      argument which sets the route METRIC value.

--route-metric METRIC
                      Specify a default metric for use with --route

--route-nopull        Do not configure routes pushed by remote server

--server-poll-timeout SECS
                      How long to wait for a response from a remote server
                      during connection setup (Default 120 seconds)

--setenv ARGS
                      Set a custom environmental variable to pass to script.
                      This takes two mandatory arguments, variable NAME
                      and VALUE.

--static-challenge ARGS
                      Enable static challenge/response protocol.  This
                      takes one mandatory option, MESSAGE, which will
                      be presented to the user before the connection
                      attempt.  An optional argument, ECHO, indicates
                      if the user input should be echoed back to the
                      user during input entry.

--tcp-queue-limit NUM
                      Maximum number of queued TCP output packets

--tls-auth ARGS
                      Enables an additional HMAC auth on TLS control channel.
                      This takes a mandatory argument, FILE, which
                      must be a shared secret between server and client.
                      The optional KEY-DIRECTION argument defines
                      which sub-key pair in FILE to use for HMAC
                      signing and verification; valid values are *0* or *1*.

--tls-cert-profile PROFILE
                      Sets certificate profile which defines acceptable
                      crypto algorithms. Valid profiles: legacy, preferred,
                      suiteb

--tls-client          Enable TLS and assume client role during TLS
                      handshake. Implicitly added when using --client

--tls-crypt FILE      Encrypts the TLS control channel with a shared secret
                      key (FILE). This CANNOT be combined with --tls-auth

--tls-timeout SECS    Packet retransmit timeout on TLS control channel if no
                      ACK from remote within n seconds (Default 2 seconds

--topology TYPE       Set tunnel topology type. Default is net30.
                      Recommended: subnet.Valid topologies: subnet, net30

--tran-window SECS    Transition window -- old data channel key can live
                      this many seconds after new after new key renegotiation
                      begins (Default 3600 secs)

--tun-mtu SIZE        Set TUN/TAP device MTU to SIZE and derive TCP/UDP from
                      it (default is 1500)

--verb LEVEL          Set log verbosity level. Log levels are NOT compatible
                      with OpenVPN 2 --verb

--verify-x509-name ARGS
                     Accept connections only with a host with a specific
                     X509 subject or CN match string.  This option takes
                     one mandatory argument, which is a MATCH string and
                     an optional match FLAG.

                     FLAG can be:

                     * *name*:
                       Match against complete X.509 Common Name field

                     * *name-prefix*:
                       The MATCH value must be match the beginning of the
                       X.509 Common Name field.  If the X.509 certificate
                       contains 'server-1.example.org', it will be a match
                       if the MATCH value is 'server-'.  It will not be a
                       match if values like 'server-2' or '.example.org' is
                       used.

                     * *subject* (default):
                       The MATCH value must be the full and complete
                       X.509 Subject field.



IGNORED OPTIONS
===============
The options in this list will be silently ignored.  Some of these options
have not yet been implemented in the OpenVPN 3 Core library and others
are not relevant any more.  But none of these options will break any
existing configurations.

--chroot DIR          Chroot to this directory after initialization. Not
                      applicable with OpenVPN 3, which uses a different
                      execution model.

--explicit-exit-notify <ATTEMPTS>
                        On exit/restart, send exit signal to remote end.
                        Automatically configured with OpenVPN 3

--group GROUP         Run OpenVPN with GROUP group credentials. Not needed
                      with OpenVPN 3 which uses a different privilege
                      separation approach
--nice LEVEL          Change process priority. Not supported in OpenVPN 3

--nobind              Do not bind to local address and port. This is default
                      behaviour in OpenVPN 3
--persist-key         Do not re-read key files across connection restarts.
                      Not needed. OpenVPN 3 keeps keys as embedded file
                      elements in the configuration

--rcvbuf SIZE         Set the TCP/UDP receive buffer size. Not supported in
                      OpenVPN 3

--resolv-retry SECS   If hostname resolve fails for --remote, retry resolve
                      for n seconds before failing. Not supported by
                      OpenVPN 3

--sndbuf SIZE         Set the TCP/UDP send buffer size. Not supported in
                      OpenVPN 3

--socket-flags FLAGS
                      Applies flags to the transport socket. Not supported
                      in OpenVPN 3

--user USER           Run OpenVPN with USER user credentials. Not needed
                      with OpenVPN 3 which uses a different privilege
                      separation approach


SEE ALSO
========

``openvpn``\(8)
``openvpn3``\(1)
``openvpn3-config-manage``\(1)
