========
openvpn2
========

--------------------------------------
OpenVPN 2 front-end to OpenVPN 3 Linux
--------------------------------------

:Manual section: 8
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

--comp-lzo [MODE]
                      Use LZO compression (Deprecated, use --compress
                      instead)

--compress [ALG]
                      Compress using algorithm ALG

--config FILE         Read configuration options from file

--daemon              Run the VPN tunnel in the background

--dev DEV-NAME        tun/tap device to use for VPN tunnel

--dev-type DEV-TYPE   Which device type are we using? tun or tap. Not needed
                      if --dev starts withtun or tap

--dhcp-option [OPTION [...]]
                      Set DHCP options which can be pickedup by the OS
                      configuring DNS, etc

--extra-certs FILE    Specify a file containing one or more PEM certs
                      (concatenated together) that complete the local
                      certificate chain.

--float               Allow remote to change its IP address/port

--hand-window SEC     Handshake window. The TLS-based key exchange must
                      finalize within SEC seconds handshake initiation by
                      any peer. (Default 60 seconds)

--http-proxy [SRV PORT [auth] [auth-method]]
                      Connect to a remote host via an HTTPproxy at address
                      SRV and port PORT. See manual for auth details

--http-proxy-user-pass FILE
                      Fetch HTTP proxy credentials from FILE

--ifconfig LOCAL NETMASK LOCAL NETMASK
                      Configure TUN/TAP device with LOCAL for local IPv4
                      address with netmask NETMASK

--ifconfig-ipv6 [LOCAL [REMOTE_ENDP] [LOCAL [REMOTE_ENDP] ...]]
                      Configure TUN/TAP device with LOCAL for local IPv6
                      address and REMOTE_ENDP as the remote end-point

--inactive [SECS [BYTES]]
                      Exit after n seconds of activity on TUN/TAP device. If
                      BYTES is added, if bytes on the device is less than
                      BYTES the tunnel will also exit

--keepalive P_SECS R_SECS P_SECS R_SECS
                      Ping remote every P_SECS second and restart tunnel if
                      no response within R_SECS seconds

--key FILE            Local private key in .pem format

--key-direction DIR   Set key direction for static keys. Valid values: 0, 1

--local HOST          Local host name or IP address to to bind against on
                      local side

--lport PORT          TCP/UDP port number for local bind (default 1194)

--mode MODE           Operational mode. Only "client" isaccepted

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

--remote HOST [PORT [PROTO]
                      Remote host or IP. PORT number and PROTO are optional.
                      May be provided multiple times.

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

--route NETWORK [NETMASK [GATEWAY [METRIC]]
                      Add route to routing table after connection is
                      established. Multiple routes can be specified. Default
                      NETMASK: 255.255.255.255. Default GATEWAY is taken
                      from --route-gateway or --ifconfig

--route-gateway [GW|dhcp]
                      Specify a default gateway for use with --route. See
                      openvpn\(8) man page for dhcp mode

--route-ipv6 NETWORK/PREFIX [GATEWAY [METRIC]]
                      Add IPv6 route to routing table after connection is
                      established. Multiple routes can be specified. Default
                      GATEWAY is taken from 'remote' in --ifconfig-ipv6

--route-metric METRIC
                      Specify a default metric for use with --route

--route-nopull        Do not configure routes pushed by remote server

--server-poll-timeout SECS
                      How long to wait for a response from a remote server
                      during connection setup (Default 120 seconds)

--setenv [NAME [VALUE]]
                      Set a custom environmental variable to pass to script.

--static-challenge MSG [ECHO]
                      Enable static challenge/response protocol using
                      challenge text MSG, with ECHO indicating echo flag
                      (0|1)

--tcp-queue-limit NUM
                      Maximum number of queued TCP output packets

--tls-auth FILE [DIR]
                      Add additional HMAC auth on TLS control channel. FILE
                      must be a shared secret. DIR is optional and defines
                      which sub-keys in FILE to use for HMAC signing and
                      verification

--tls-cert-profile PROFILE
                      Sets certificate profile which defines acceptable
                      crypto algorithms. Valid profiles: legacy, preferred,
                      suiteb

--tls-client          Enable TLS and assume client role during TLS
                      handshake. Implicitly added when using --client

--tls-crypt FILE      Encrypts the TLS control channel with a shared secret
                      key (FILE). ThisCANNOT be combined with --tls-auth

--tls-timeout SECS    Packet retransmit timeout on TLS control channel if no
                      ACK from remote within n seconds (Default 2 seconds

--topology TYPE       Set tunnel topology type. Default is net30.
                      Recommended: subnet.Valid topologies: subnet, net30

--tran-window SECS    Transition window -- old data channel key can live
                      this many seconds after newafter new key renegotiation
                      begins (Default 3600 secs)

--tun-mtu SIZE        Set TUN/TAP device MTU to SIZE and derive TCP/UDP from
                      it (default is 1500)

--verb LEVEL          Set log verbosity level. Log levels are NOT compatible
                      with OpenVPN 2 --verb


IGNORED OPTIONS
===============
The options in this list will be silently ignored.  Some of these options
have not yet been implemented in the OpenVPN 3 Core library and others
are not relevant any more.  But none of these options will break any
existing configurations.

--chroot DIR          Chroot to this directory after initialization. Not
                      applicable with OpenVPN 3, which uses a different
                      execution model.

--explicit-exit-notify [ATTEMPTS]
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

--verify-x509-name SUBJECT [FLAGS] [SUBJECT [FLAGS] ...]
                      Accept connections only with a host with X509 subject.
                      Not yet implemented in OpenVPN 3


SEE ALSO
========

``openvpn``\(8)
``openvpn3``\(8)

