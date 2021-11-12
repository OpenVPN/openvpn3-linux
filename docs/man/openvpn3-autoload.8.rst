=================
openvpn3-autoload
=================

------------------------------------------------------
Automated OpenVPN 3 Linux configuration profile loader
------------------------------------------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-autoload`` [ OPTIONS ]
| ``openvpn3-autoload`` ``-h`` | ``--help``


DESCRIPTION
===========
The **openvpn3-autoload** utility is used to automatically load and
optionally start VPN configuration profiles from a specific directory.
As the OpenVPN 3 Linux client provides fine grained control on both
configuration profiles and VPN sessions which are managed outside the
main configuration profile, the configuration profiles must be accompanied
by a *.autoload* definition with local site-specific settings.

The **openvpn3-autoload** utility is provided with a
**openvpn3-autoload.service** *systemd* unit file which can be enabled and
will load configuration profiles located in */etc/openvpn3/autoload*.

OPTIONS
=======

-h, --help           Prints usage information and exits.
--directory DIR      Required.  Directory to look for configuration
                     profiles
--ignore-autostart   Optional.  Do not automatically start any VPN sessions
                     which have been configured to start during loading.


BACKGROUND
==========
Traditionally in the OpenVPN 2.x, configuration profiles can contains a lot
of options which are both site/host independent and site/host specific.
This creates a challenge when distributing configuration profiles, as a
profile working for one device in one particular network might not work
on a different device or network.

OpenVPN 3 provides a way how to split out the site/host dependent options
from the main VPN configuration profile being provided by the VPN
provider.  In the OpenVPN 3 Linux client, these site/host specific settings
are managed via *openvpn3 config-manage* for configuration profiles
already imported.  The *.autoload* configuration files is used to automate
setting up these various options without any direct user interaction.


FILE FORMAT
===========
The file format is based on JSON formatting and will carry the
:code:`.autoload` extension instead of :code:`.ovpn` or :code:`.conf`.
The :code:`.autoload` file must be located in the same directory as the
main configuration file.  The base part of filename must be identical with the
:code:`.ovpn`/:code:`.conf` file.  Example: If the configuration profile is
named :code:`vpn-client.conf`, the :code:`.autoload` file must be named
:code:`vpn-client.autoload`.

Main section
~~~~~~~~~~~~~

The basic layout of an :code:`.autoload` file is like this:

::

    {
       "autostart": BOOLEAN,
        "name": "string value",
        "acl": {
            ...
        },
        "crypto": {
            ...
        },
        "remote": {
            ...
        },
        "tunnel": {
            ...
        },
        "user-auth": {
            ...
        }
    }


Attribute: autostart
""""""""""""""""""""
The :code:`autostart` boolean declares if the configuration profile should be
started once it has been imported into the OpenVPN 3 Configuration Manager.
(Default: :code:`false`)

Attribute: name
"""""""""""""""
By default, all automatically imported configuration profiles will use the
complete profile filename, including the :code:`.conf` or :code:`.ovpn` file
extension.  If this attribute is set, this string will be used for the profile
name instead of the filename on the filesystem.  Beware that the configuration
manager will accept duplicate profile names.

Section: acl
~~~~~~~~~~~~

The :code:`acl` section declares several Access Control Level parameters of
the imported configuration profile.  Valid settings are:
::

    "acl": {
        "public": BOOLEAN,
        "locked-down": BOOLEAN,
        "set-owner": UID
    }

Attribute acl:public
""""""""""""""""""""

The :code:`public` element declares if this configuration profile is available
for all users on the system or not.  (Default: :code:`false`)

Attribute: acl:locked-down
""""""""""""""""""""""""""
By setting the :code:`locked-down` element to :code:`true`, users granted
access can only start new tunnels with this profile but cannot look look at
the information stored in the configuration profile. (Default: :code:`false`)

Attribute: acl:set-owner
""""""""""""""""""""""""
By default all processed configuration profiles will be owned by the user
who runs **openvpn3-autoload**.  The root user on the system can re-assign
the ownership of configuration profiles it imports, like when running this
utility during the system boot.  By providing the :code:`set-owner` element
with the UID of the user who should own this configuration profile, the
ownership will be transferred.  This is a feature only available by root.


Section: crypto
~~~~~~~~~~~~~~~
The :code:`crypto` section enables fine-tuning some of the configuration
parameters related to the crypto layers of a VPN session.

::

    "crypto": {
        "tls-params": {
            ...
        }
    }


Sub-Section: crypto:tls-params
""""""""""""""""""""""""""""""
The :code:`tls-params` sub-section further controls the TLS protocol parameters.

::

    "tls-params": {
        "cert-profile": [ "legacy" | "preferred" | "suiteb" ],
        "min-version": [ "disabled" | "default" | "tls_1_0" | "tls_1_1" | "tls_1_2" | "tls_1_3" ]
    }

Attribute: crypto:tls-params:cert-profile
""""""""""""""""""""""""""""""""""""""""""
The :code:`cert-profile` declares the security level of the TLS channel.  Valid
values are:

:code:`legacy`
    Allows minimum 1024 bits RSA keys with certificates signed with SHA1.

:code:`preferred`
    Allows minimum 2048 bits RSA keys with certificates signed with
    SHA256 or higher.

:code:`suiteb`
    This follows the NSA Suite-B specification.

Attribute: crypto:tls-params:min-version
""""""""""""""""""""""""""""""""""""""""
The :code:`min-version` defines the minimum TLS version being accepted by the
client.  Valid values are:

:code:`disabled`
    No minimum version is defined nor required

:code:`default`
    Uses the default minimum version the SSL library defines

:code:`tls_1_0`
    Requires at least TLSv1.0

:code:`tls_1_1`
    Requires at least TLSv1.1

:code:`tls_1_2`
    Requires at least TLSv1.2

:code:`tls_1_3`
    Requires at least TLSv1.3


Section: remote
~~~~~~~~~~~~~~~
Settings in this section is related to the connection to the remote
server.  It does not support different settings per remote server but
is shared for all the remote servers enlisted in the main
configuration file.

::

     "remote": {
             "proto-override": [ "udp" | "tcp" ],
             "port-override": PORT_NUM,
             "timeout": SECONDS,
             "compression": [ "no" | "yes" | "asym" ],
             "proxy": {
                 ...
             }
     }

Attribute: remote:protocol-override
"""""""""""""""""""""""""""""""""""
This forces the VPN client to connect using the given protocol.  Valid
values are :code:`tcp` or :code:`udp`.

Attribute remote:port-override
""""""""""""""""""""""""""""""
Port number to use instead of the port number defined in the VPN
configuration profile.  It must be an integer between :code:`0` and
:code:`65535`.

Attribute: remote:timeout
"""""""""""""""""""""""""
An unsigned integer defining how long to wait before trying the next
remote entry enlisted in the VPN configuration profile.

Attribute: remote:compression
"""""""""""""""""""""""""""""
Controls how compression settings for the data channel.  Valid values are:

:code:`no`
    Compression is disabled

:code:`yes`
    Compression is enabled in both directions

:code:`asym`
    Compression is only enabled for traffic sent from the remote side to
    the local side.


Sub-section: remote:proxy
~~~~~~~~~~~~~~~~~~~~~~~~~
This sub-section configures the client to start the connection via an HTTP
proxy server.

::

             "proxy": {
                     "host": "proxy-server-name",
                     "port": "proxy-port",
                     "username": "proxy-username",
                     "password": "proxy-password",
                     "allow-plain-text": BOOLEAN
             }

Attribute: remote:proxy:host
""""""""""""""""""""""""""""
String containing the hostname of the HTTP proxy


Attribute: remote:proxy:port
""""""""""""""""""""""""""""
Unsigned integer defining the port to use when connecting to the proxy
server

Attribute: remote:proxy:username
""""""""""""""""""""""""""""""""
If the proxy server requires user authentication, this need to contain
a string with the proxy username to use.

Attribute: remote:proxy:password
""""""""""""""""""""""""""""""""
If the proxy server requires user authentication, this need to contain
a string with the password to use.

Attribute: remote:proxy:allow-plain-text
""""""""""""""""""""""""""""""""""""""""
Boolean flag enabling or disabling the OpenVPN 3 client to transport
the proxy username/password unencrypted.  Default: :code:`false`


Section: tunnel
~~~~~~~~~~~~~~~
The tunnel section defines settings related to the tunnel interface.
On some platforms this interacts directly with a tun/tap interface
while other platforms may pass these settings via VPN API provided by
the platform.

::

     "tunnel": {
             "ipv6": [ "yes" | "no" | "default" ],
             "persist": BOOLEAN,
             "dns-fallback": [ "google" ],
             "dns-setup-disabled": BOOLEAN
         }

Attribute: tunnel:ipv6
""""""""""""""""""""""
Enable or disable the IPv6 capability on the tunnel interface.  This
can be a string which must contain one of these values:

:code:`yes`
    IPv6 capability is enabled and will be configured if
    the server sends IPv6 configuration details

:code:`no`
    IPv6 capability is disabled and will not be configured,
    regardless of what the server provides of IPv6 configuration details

:code:`default`
    Make use of IPv6 if the platform supports it

Attribute: tunnel:persist
"""""""""""""""""""""""""
Boolean flag which enables the persistent tunnel interface behaviour.  This
is related to whether the tunnel interface will be torn down and
re-established during re-connections or restarts of the VPN tunnel.
If set to :code:`true`, the tunnel interface is preserved during such events.

Attribute: tunnel:dns-fallback
""""""""""""""""""""""""""""""
This makes the VPN client configure an additional fallback DNS
server on the system.  Valid strings are:

:code:`google`
    Configures the system to use :code:`8.8.8.8` and :code:`8.8.4.4`
    as fallback DNS servers

Attribute: dns-scope
""""""""""""""""""""
Defines the DNS query scope.  This is currently only supported when enabling
the `systemd-resolved`\(8) resolver support in `openvpn3-service-netcfg`\(8).
Supported values are:

:code:`global`:  (default)
    The VPN service provided DNS server(s) will be used for all types of
    DNS queries.

:code:`tunnel`:
    The VPN service provided DNS server(s) will only be used for queries for
    DNS domains pushed by the VPN service.

    **NOTE**
        The DNS domains pushed by the VPN service may be queried by DNS
        servers with `systemd-resolved`\(8) service if their respective
        interfaces are configured to do global DNS queries.  But other
        non-listed DNS domains will not be sent to this VPN service
        provider's DNS server.


Attribute: dns-setup-disabled
"""""""""""""""""""""""""""""
Controls whether DNS configurations in the VPN configuration profile or
DNS settings sent from the server will be applied on the system or not.
(Default: :code:`false`)


Section: user-auth
~~~~~~~~~~~~~~~~~~
This section is only important if the server uses user authentication
methods other than certificate based authentication and this section is
only used if the :code:`autostart` attribute is set to :code:`true`.
This is used to automate the client connection as much as possible.

::

     "user-auth": {
         "autologin": BOOLEAN,
         "username": "string value",
         "password": "string value",
         "pk_passphrase": "string value",
         "dynamic_challenge": "string value"
     }


Attribute: user-auth:autologin
""""""""""""""""""""""""""""""
If set to :code:`true`, the client will not ask for username/password as it is
expected that the VPN configuration profile carries the needed settings
providing the identity towards the server.  (Default: :code:`false`)

Attribute: user-auth:username
"""""""""""""""""""""""""""""
String containing the username to authenticate as.

Attribute: user-auth:password
"""""""""""""""""""""""""""""
String containing the password used for the authentication.

Attribute: user-auth:pk_passphrase
""""""""""""""""""""""""""""""""""
String containing the private key passphrase, which is needed if the
private key in the VPN configuration profile is encrypted.

Attribute: user-auth:dynamic_challenge
""""""""""""""""""""""""""""""""""""""
The server might ask the client for a dynamic challenge.  If the expected
response is static, the static response can be put here.  If the server
expects an OTP token code or similarly dynamic changing input, the
VPN configuration profile is not suitable for :code:`autostart`.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-manage``\(1)

