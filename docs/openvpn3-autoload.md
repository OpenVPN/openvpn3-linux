OpenVPN 3 Autoload feature
==========================

The OpenVPN configuration files contains a lot of options which
attempts to be as site/host independent as possible.  Options found
here should be functional across any device using the configuration.
In OpenVPN 2.x there are several options which also covers these
environment specific settings; while OpenVPN 3 tries to move as much
of these options outside of the main configuration file.

This new autoloader file format targets these environment specific
options and is to be used when configuration files are automatically
loaded, often during system boot or when a user is logging in.

This new file format is based on JSON formatting and will carry the
`.autoload` extension instead of `.ovpn` or `.conf`.  This `.autoload`
file is to be located in the same directory as the main configuration
file.  The base part of filename must be identical with the
`.ovpn`/`.conf` file.

An `.autoload` file consists of several sections and all sections and
properties within any section are optional.

## WARNING!
This spec is under development and it may change.
The reference implementation at any time will be the
`openvpn3-autoload` Python script.  And there might be features
described in this document which is not implemented yet.

## Main section: autostart

This is a single boolean flag which enables the configuration to be
automatically started when being processed.  If this is not set or set
to false, it will just load the configuration file and prepare
everything to be activated later on by a user or through a system
event later on, outside of the `openvpn3-autoload` process.

## Main section: name

By default, the complete configuration profile file name is used when
importing the profile, which will include the `.conf` or `.ovpn` file
extension.  If this attribute is set, it will be used instead of the file
name itself for the configuration profile name in the configuration manager.

## Section: user-auth
This section contains authentication related settings.

    "user-auth": {
        "autologin": BOOLEAN,
        "$VARNAME": "VALUE"
    }

#### user-auth: autologin
**TBD**

#### user-auth: $VARNAME
The OpenVPN 3 `UserInputQueue` API uses an internal variable name in
all of its requests.  A request for username and password can look
something like this pseudo array-struct when using the D-Bus interface
directly:

    [{
        type = (uint) 1;  // ClientAttentionType::CREDENTIALS
        group = (uint) 1; // ClientAttentionGroup::USER_PASSWORD
        id = (uint) 1;
        name = (string) "username";  // This is the $VARNAME
        description = (string) "Auth Username";
        hidden_input = (bool) false;
     },
     {
        type = (uint) 1;  // ClientAttentionType::CREDENTIALS
        group = (uint) 1; // ClientAttentionGroup::USER_PASSWORD
        id = (uint) 2;
        name = (string) "password";  // This is the $VARNAME
        description = (string) "Auth Password";
        hidden_input = (bool) true;
    }]

For an `.autoload` configuration to work with this example request
above, it would need to look like this:

     "user-auth": {
         "autologin": true,
         "username": "user1",
         "password": "a-secret-password"
    }

It is considered a failure if the `.autoload` configuration is missing
information which is present in the `UserInputQueue` requests.

## Section: remote
Settings in this section is related to the connection to the remote
server.  It does not support different settings per remote server but
is shared for all the remote servers enlisted in the main
configuration file.

    "remote": {
            "proto-override": "PROTOCOL",
            "port-override": PORT_NUM,
            "timeout": SECONDS,
            "compression": "COMPRESSION",
            "proxy": {
                    "host": "proxy-server-name",
                    "port": "proxy-port",
                    "username": "proxy-username",
                    "password": "proxy-password",
                    "allow-plain-text": BOOLEAN
            }
    }

#### remote:protocol-override
A string containing either `"tcp"` or `"udp"`.

#### remote: port-override
Port number to use instead of the port
number defined in the main configuration.  It must be an integer
between `0` and `65535`.

#### remote: timeout
An unsigned integer defining how long to wait
before trying the next remote entry enlisted in the main configuration
file.

#### remote: compression
A string which can contain:

 - `"no"`:  Compression is disabled
 - `"yes"`: Compression is enabled in both directions
 - `"asym"`: Compression is only enabled in traffic coming from the
             server going to the client

### remote: proxy
This sub-section configures the client to start the connection via an
HTTP proxy

#### proxy: host
String containing the hostname of the proxy

#### proxy: port
Unsigned integer defining the port to use when connecting to the proxy
server

#### proxy: username
If the proxy server requires user authentication, this need to contain
a string with the proxy username to use.

#### proxy: password
If the proxy server requires user authentication, this need to contain
a string with the password to use.

#### proxy: allow-plain-text
Boolean flag enabling or disabling the OpenVPN 3 client to transport
the proxy username/password unencrypted.

### Section: crypto

    "crypto": {
            "default-key-direction": KEY_DIR,
            "private-key-passphrase": "PASSWORD",
            "tls-params": {
                    "cert-profile": "PROFILE",
                    "min-version": "TLS_VERSION"
            }
    }

#### crypto: default-key-direction
This can be either `-1`, `0` or `1`.  Key direction is related to
which sub-key to use when `--tls-auth` or `--tls-crypt` is enabled.

The default (`-1`) is bi-directional mode where both sides uses the
same key.  Otherwise the client must use either 0 or 1 while the
server side uses 1 or 0; the local side must use the opposite value of
what the remote side is configured to use.

#### crypto: private-key-passphrase
A string containing the password to decrypt the private key, if it is
encrypted.

### crypt: tls-params
This sub-section defines the TLS protocol specific parameters

#### tls-params: cert-profile
This is a string which defines the level of security the client will
allow the TLS protocol to use.  Valid values are:

 - `"legacy"`:  This allows 1024 bits RSA keys with certificate
                part signed with SHA1
 - `"preferred"`: This requires minimum 2048 bits RSA key with
                certificate signed with SHA256 or higher
 - `"suiteb"`: This follows the NSA Suite-B specification

#### tls-params: min-version
This is a string defining the minimum version of the TLS protocol to
accept.  Valid values are:

 - `"disabled"`:  No minimum version is defined nor required
 - `"default"`: Uses the default minimum version the SSL library
                defines
 - `"tls-1.0"`: Requires at least TLSv1.0
 - `"tls-1.1"`: Requires at least TLSv1.1
 - `"tls-1.2"`: Requires at least TLSv1.2

## Section: tunnel
The tunnel section defines settings related to the tunnel interface.
On some platforms this interacts directly with a tun/tap interface
while other platforms may pass these settings via VPN API provided by
the platform.

    "tunnel": {
            "ipv6": "IPV6_SETTING",
            "persist": BOOLEAN,
            "dco": BOOLEAN,
            "dns-fallback": "PROVIDER",
            "dns-setup-disabled": BOOLEAN
	}

#### tunnel: ipv6
Enable or disable the IPv6 capability on the tunnel interface.  This
can be a string which must contain one of these values:

 - `"yes"`: IPv6 capability is enabled and will be configured if
            the server sends IPv6 configuration details
 - `"no"`: IPv6 capability is disabled and will not be configured,
           regardless of what the server provides of IPv6
           configuration details
 - `"default"`: Make use of IPv6 if the platform supports it

#### tunnel: persist
Boolean flag enabling the persistent tunnel interface behaviour.  This
is related to whether the tunnel interface should be torn down and
re-established during re-connections or restarts of the VPN tunnel.
If set to true, the tunnel interface is preserved during such events.
This setting may not be supported on all platforms.

#### tunnel: dco
Boolean flag enabling the Data Channel Offload (DCO).  This moves the
encryption and decryption of packets sent to the VPN tunnel to be
processed in kernel space instead of being transported to user space
before being sent to the remote side or put on the local tunnel
interface.

**PLEASE NOTE!** Data Channel Offload is only available on
Linux and requires a specific DCO kernel module to be loaded to be
functional.  Without this kernel module the configuration will not
start properly if `dco` is enabled.

#### tunnel: dns-fallback
This makes the VPN client to configure an additional fallback DNS
server on the system.  Valid strings are:

 - `"google"`:  Configures the system to use 8.8.8.8 and 8.8.4.4
                as fallback DNS servers

#### tunnel: dns-setup-disabled
Setting this to True will disable modifying the local DNS for this
configuration profile when the tunnel is started.


## Section: log

    "log": {
            "level": LOG_LEVEL,
            "destination": {
                    "service":  "SERVICE",
                    [... service specific settings ...]
        }
    }

#### log: level
Defines the log verbosity level to be logged.  The higher the value
is, the more verbose the logging will be.  Valid log levels are `0` to
`6`, where `6` will only be available if debug logging has been
enabled.

#### log: destination
This sub-section will contain different types of settings, depending
on the defined service.  The "service" property is required and must
be a string where the following values are accepted:

 - `"file"`
 - `"syslog"`
 - `"journal"`
 - `"windows-event"`


### Log service: file
The "file" log service is the most primitive one, which does all
logging to a defined file.

	{
        "service": "file",
        "destination": "FILENAME",
        "log-rotate-size": INTEGER
    }

#### file - Log Service Property: destination
This is a file path where to write log data.  If no full path is
provided, it will use the current directory of where the VPN client
was started

#### file - Log Service Property: log-rotate-size
This is an unsigned integer defining the maximum size (in MB) of the
log file can grow before it gets rotated.

### Log service: syslog
The "syslog" log service uses the standard syslog interface most
commonly available on the Unix based platforms

    {
        "service": "syslog",
        "facility": "FACILITY"
    }

#### syslog - Log Service Property: facility
This is a string defining which syslog facility to send the log data
to.  Valid values are defined by the openlog() function.  Most common
values will most likely be:

 - "`LOG_DAEMON`" (default)
 - "`LOG_LOCAL0`" ... "`LOG_LOCAL7`"
 - "`LOG_SYSLOG`"
 - "`LOG_USER`"


### Log service: journal
**TBD**

This is Linux specific and it will send log data directly to the
systemd journal.

    {
        "service": "journal",
	}

#### Log service: windows-event
**TBD**

This is Windows specific and it will send log data directly to the
Windows Event Log

	{
        "service": "windows-event",
	}

## Section: acl
This defines some primitive access control parameters to the
configuration.  This is most commonly needed on systems where the
configuration may be accessible to local users via services on the
system.  This feature may not be available on all platforms and they
are not directly related to features inside the OpenVPN 3 Core
Library.

    "acl": {
        "set-owner": UID,
        "public": BOOLEAN,
        "locked-down": BOOLEAN
    }

#### acl: set-owner
By default all imported configurations and automatically started
sessions are owned by root.  By setting this to a different UID
value (numeric), the imported configuration profile will be set
to the provided UID.  If a session is also automatically started,
the owner of this session is also set to the same value.

#### acl: public
This is a boolean flag enabling all users on the local system to
start/stop or somewhat read or in some cases modify the configuration
profile or its `.autoload` settings.  If not set or set to false, this
configuration should only be accessible by the user account which
loaded it.

#### acl: locked-down
A boolean flag allowing the access to this configuration to be far
more restrictive.  A locked down profile will effectively only make
other users start and stop the tunnel.  It may retrieve some of
the insensitive `.autoload` settings but will not have access to the
main configuration file directly.
