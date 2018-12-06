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
by a *.autoload* definition with local site-specific details.

The **openvpn3-autoload** utility is provided with a
**openvpn3-autoload.service** *systemd* unit file which can be enabled and
will load configuration profiles located in */etc/openvpn3/autoload*.

OPTIONS
=======

-h, --help           Prints usage infromation and exits.
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
*.autoload* extension instead of *.ovpn* or *.conf*.  The *.autoload*
file is to be located in the same directory as the main configuration
file.  The base part of filename must be identical with the
*.ovpn*/*.conf* file.  Example: If the configuration profile is named
*vpn-client.conf*, the *.autoload* file must be named
*vpn-client.autoload*.

Main section
~~~~~~~~~~~~~

The basic layout of an *.autoload* file is like this:

|
|   {
|       "autostart": BOOLEAN,
|       "acl": {
|           ...
|       },
|       "crypto": {
|           ...
|       },
|       "remote": {
|           ...
|       },
|       "tunnel": {
|           ...
|       },
|       "user-auth": {
|           ...
|       }
|   }


Attribute: autostart
""""""""""""""""""""
The *autostart* boolean declares if the configuration profile should be
started once it has been imported into the OpenVPN 3 Configuration Manager.
(Default: false)


Section: acl
~~~~~~~~~~~~

The *acl* section declares several Access Control Level parameters of
the imported configuration profile.  Valid settings are:

|
|   "acl": {
|       "public": BOOLEAN,
|       "locked-down": BOOLEAN,
|       "set-owner": UID
|   }

Attribute acl:public
""""""""""""""""""""

The *public* element declares if this configuration profile is available
for all users on the system or not.  (Default: false)

Attribute: acl:locked-down
""""""""""""""""""""""""""
By setting the *locked-down* element to true, users granted access can
only start new tunnels with this profile but cannot look look at the
information stored in the configuration profile. (Default: false)

Attribute: acl:set-owner
""""""""""""""""""""""""
By default all processed configuration profiles will be owned by the user
who runs **openvpn3-autoload**.  The root user on the system can re-assign
the ownership of configuration profiles it imports, like when running this
utility during the system boot.  By providing the "set-owner" element with
the UID of the user who should own this configuration profile, the
ownership will be transfered.  This is a feature only available by root.


Section: crypto
~~~~~~~~~~~~~~~
The *crypto* section enables fine-tuning some of the configuration
parameters related to the crypto layers of a VPN session.

|
|   "crypto": {
|       "client-cert-enabled": BOOLEAN,
|       "force-aes-cbc": BOOLEAN,
|       "tls-params": {
|           ...
|       }
|   }
|

Attribute: crypto:cert-enabled
""""""""""""""""""""""""""""""
*client-cert-enabled* is enabled by default, which requires the VPN profile
to contain a client certificate.  By setting this to false, the VPN client
will presume other types of authentication being enabled, such as
username/password authentication.

Attribute: crypto:force-aes-cbc
"""""""""""""""""""""""""""""""
The *force-aes-cbc* elements enforces the use of the AES-CBC cipher
algorithm.  This is disabled by default, which allows the configuration
profile to control the cipher, or can allow the server to change the
cipher via the Negotiable Crypto Parameters protocol (NCP).


Sub-Section: crypto:tls-params
""""""""""""""""""""""""""""""
The *tls-params* sub-section further controls the TLS protocol parameters:

|
|   "tls-params": {
|       "cert-profile": [ "legacy" | "preferred" | "suiteb" ],
|       "min-version": [ "disabled" | "default" | "tls_1_0" | "tls_1_1" | "tls_1_2" | "tls_1_3" ]
|   }

Attribute: crypto:tls-params:cert-profile
""""""""""""""""""""""""""""""""""""""""""
The *cert-profile* declares the security level of the TLS channel.  Valid
values are:

   * *legacy*
     Allows minimum 1024 bits RSA keys with certificates signed with SHA1.

   * *preferred*
     Allows minimum 2048 bits RSA keys with certificates signed with
     SHA256 or higher.

   * *suiteb*
     This follows the NSA Suite-B specification.

Attribute: crypto:tls-params:min-version
""""""""""""""""""""""""""""""""""""""""
The *min-version* defines the minimum TLS version being accepted by the
client.  Valid values are:

   * *disabled*
     No minimum version is defined nor required

   * *default*
     Uses the default minimum version the SSL library defines

   * *tls_1_0*
     Requires at least TLSv1.0

   * *tls_1_1*
     Requires at least TLSv1.1

   * *tls_1_2*
     Requires at least TLSv1.2

   * *tls_1_3*
     Requires at least TLSv1.3


Section: remote
~~~~~~~~~~~~~~~
Settings in this section is related to the connection to the remote
server.  It does not support different settings per remote server but
is shared for all the remote servers enlisted in the main
configuration file.

|
|    "remote": {
|            "proto-override": [ "udp" | "tcp" ],
|            "port-override": PORT_NUM,
|            "timeout": SECONDS,
|            "compression": [ "no" | "yes" | "asym" ],
|            "proxy": {
|                ...
|            }
|    }

Attribute: remote:protocol-override
"""""""""""""""""""""""""""""""""""
This forces the VPN client to connect using the given protocol.  Valid
values are *tcp* or *udp*.

Attribute remote:port-override
""""""""""""""""""""""""""""""
Port number to use instead of the port number defined in the VPN
configuration profile.  It must be an integer between *0* and *65535*.

Attribute: remote:timeout
"""""""""""""""""""""""""
An unsigned integer defining how long to wait before trying the next
remote entry enlisted in the VPN configuration profile.

Attribute: remote:compression
"""""""""""""""""""""""""""""
Controls how compression settings for the data channel.  Valid values are:

   * *no*
     Compression is disabled

   * *yes*
     Compressoin is enanbled in both directions

   * *asym*
     Compression is only enabled for traffic sent from the remote side to
     the local side.


Sub-section: remote:proxy
~~~~~~~~~~~~~~~~~~~~~~~~~
This sub-section configures the client to start the connection via an HTTP
proxy server.

|
|            "proxy": {
|                    "host": "proxy-server-name",
|                    "port": "proxy-port",
|                    "username": "proxy-username",
|                    "password": "proxy-password",
|                    "allow-plain-text": BOOLEAN
|            }

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
the proxy username/password unencrypted.  Default: false


Section: tunnel
~~~~~~~~~~~~~~~
The tunnel section defines settings related to the tunnel interface.
On some platforms this interacts directly with a tun/tap interface
while other platforms may pass these settings via VPN API provided by
the platform.

|
|    "tunnel": {
|            "ipv6": [ "yes" | "no" | "default" ],
|            "persist": BOOLEAN,
|            "dns-fallback": [ "google" ],
|            "dns-setup-disabled": BOOLEAN
|        }

Attribute: tunnel:ipv6
""""""""""""""""""""""

Enable or disable the IPv6 capability on the tunnel interface.  This
can be a string which must contain one of these values:

  * *yes*
    IPv6 capability is enabled and will be configured if
    the server sends IPv6 configuration details

  * *no*
    IPv6 capability is disabled and will not be configured,
    regardless of what the server provides of IPv6 configuration details

  * *default*
    Make use of IPv6 if the platform supports it

Attribute: tunnel:persist
"""""""""""""""""""""""""
Boolean flag which enables the persistent tunnel interface behaviour.  This
is related to whether the tunnel interface will be torn down and
re-established during re-connections or restarts of the VPN tunnel.
If set to true, the tunnel interface is preserved during such events.

Attribute: tunnel:dns-fallback
""""""""""""""""""""""""""""""
This makes the VPN client configure an additional fallback DNS
server on the system.  Valid strings are:

  * *google*
    Configures the system to use 8.8.8.8 and 8.8.4.4 as fallback
    DNS servers

Attribute: dns-setup-disabled
"""""""""""""""""""""""""""""
Controls whether DNS configurations in the VPN configuration profile or
DNS settings sent from the server will be applied on the system or not.
(Default: false)


Section: user-auth
~~~~~~~~~~~~~~~~~~
This section is only important if the server uses user authentication
methods other than certificate based authentication and this section is
only used if the *autostart* attribute is set to *true*.  This is used
to automate the client connection as much as possible.

|
|    "user-auth": {
|        "autologin": BOOLEAN,
|        "username": "string value",
|        "password": "string value",
|        "pk_passphrase": "string value",
|        "dynamic_challenge": "string value"
|    }


Attribute: user-auth:autologin
""""""""""""""""""""""""""""""
If set to *true*, the client will not ask for username/password as it is
expected that the VPN configuration profile carries the needed settings
providing the identity towards the server.  (Default: false)

Attribute: user-auth:username
"""""""""""""""""""""""""""""
String containing the username to authenticate as.

Attribute: user-auth:username
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
VPN configuration profile is not suitable for *autostart*.


SEE ALSO
========

``openvpn3``\(8)
``openvpn3-config-manage``\(8)

