OpenVPN 3 D-Bus API: Backend VPN client process
===============================================

The VPN backend client process (`openvpn-service-client`) is the core
VPN client in the OpenVPN 3 Linux implementation. This process
represents a single TUN/TAP adapter on the system and is the process
which implements the OpenVPN 3 Core library's VPN client object.

This process is started by the
[`net.openvpn.v3.backends`](dbus-service-net.openvpn.v3.backends.md)
service. The session manager
([`net.openvpn.v3.sessions`](77070442.html)) is the service which
calls the `net.openvpn.v3.backends.StartClient` method with a token
value, which results in a new VPN backend client process being started.

Upon initialisation, the VPN backend process asks for a well-known bus
name starting with `net.openvpn.v3.backends.be` and then appends its
own PID value. This is to ensure each VPN backend client process can
be reached by the session manager. For the session manager to know
which session object is related to which backend client process, the
VPN backend client issues the
`net.openvpn.v3.backends.RegistrationRequest` signal where it provides
its own well-known bus name in the signal. The session manager listens
for these signals and once received, the session manager responds back
by calling backend process'
`net.openvpn.v3.backends.RegistrationConfirmation` method where the
token value the backend process was started with is validated, to
ensure the proper session object is tied to the proper session object.

By design, it is only the `openvpn` user account which is allowed to
contact and call methods in the backend VPN client processes and it is
intended that only the session manager is the service which
establishes contact with this service. Any other process or user
should do all calls via the session manager. The session manager will
proxy those calls to the proper backend. And signals from the backend
will be proxied back via the session manager.


D-Bus destination: `net.openvpn.v3.backends.be${PID}` \- Object path: `/net/openvpn/v3/backends/session`
-------------------------------------------------------------------------------------------------------------

```
node /net/openvpn/v3/backends/session {
  interface net.openvpn.v3.backends {
    methods:
      RegistrationConfirmation(in  s token,
                               in  o config_path,
                               out s config_name);
      Ping(out b alive);
      Ready();
      Connect();
      Pause(in  s reason);
      Resume();
      Restart();
      Disconnect();
      ForceShutdown();
      UserInputQueueGetTypeGroup(out a(uu) type_group_list);
      UserInputQueueFetch(in  u type,
                          in  u group,
                          in  u id,
                          out u type,
                          out u group,
                          out u id,
                          out s name,
                          out s description,
                          out b hidden_input);
      UserInputQueueCheck(in  u type,
                          in  u group,
                          out au indexes);
      UserInputProvide(in  u type,
                       in  u group,
                       in  u id,
                       in  s value);
    signals:
      StatusChange(u code_major,
                   u code_minor,
                   s message);
      Log(u group,
          u level,
          s session_token,
          s message);
      AttentionRequired(u type,
                        u group,
                        s message);
      RegistrationRequest(s busname,
                          s token);
    properties:
      readwrite u log_level;
      readonly s session_name;
      readonly a{sx} statistics;
      readonly (uus) status;
      readwrite b dco;
      readonly o device_path;
      readonly s device_name;
  };
};
```

### Method: `net.openvpn.v3.backends.RegistrationConfirmation`

This method must be called by the session manager after the backend
VPN client process have issued the `RegistrationRequest` signal. The
backend client will verify the token value with the token value the
process was started with.  If it matches, this call will return boolean
true.

#### Arguments

| Direction | Name         | Type        | Description                                                |
|-----------|--------------|-------------|------------------------------------------------------------|
| In        | token        | string      | This token is used to verify that the session manager have connected the proper backend client service with the correct session object |
| In        | config_path  | object path | Contains the VPN configuration profile path to use for this connection |
| Out       | config_name  | string      | On success, contains the configuration profile name used in the configuration manager when fetching the profile |


### Method: `net.openvpn.v3.backends.Ping`

Used to check if the backend process is alive and responsive.  This
Ping method is going a bit deeper than the
`org.freedesktop.DBus.Peer.Ping`, as it ensures the response comes
from within the OpenVPN 3 code base.

#### Arguments

| Direction | Name         | Type        | Description                     |
|-----------|--------------|-------------|---------------------------------|
| Out       | alive        | boolean     | This should always return true. |


### Method: `net.openvpn.v3.backends.Ready`

Used to check if the backend process is ready to connect to the
server. This function does not return anything if it is ready. If it
is not ready, it will result in a D-Bus error response.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.backends.Connect`

This method starts the connect process to the remote server.

#### Arguments

(No arguments)

### Method: `net.openvpn.v3.backends.Pause`

Pauses an on-going VPN connection. If connection have not started or
it have already been paused, it will result in a D-Bus error response.

#### Arguments

| Direction | Name   | Type         | Description                                                                    |
|-----------|--------|--------------|--------------------------------------------------------------------------------|
| In        | reason | string       | A string used for log purposes primarily, describing why the tunnel was paused |


### Method: `net.openvpn.v3.backends.Resume`

Resumes a paused VPN connection. If the connection have not been
paused already, it will result in a D-Bus error response.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.backends.Disconnect`

Disconnects from a server. If there is no active connection running,
it will result in a D-Bus error response.  This will also do a
graceful shutdown of the VPN backend process.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.backends.ForceShutdown`

Forces the background VPN client process to stop running. It will
either shutdown properly or do a kill() by itself, depending on what
is possible. It can be expected that calling this method once will
result in the backend client process to be stopped.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.backends.UserInputQueueGetTypeGroup`

This will return information about various `ClientAttentionType`
and `ClientAttentionGroup` tuples which contains requests for the
front-end application.  This information is used when checking
the request queue via `UserInputQueueCheck`.

Valid `ClientAttentionType` values are:

| Identifier        | ID  | Description                                                        |
|-------------------|:---:|--------------------------------------------------------------------|
| UNSET             | 0   | This is an invalid value, used for initialization only             |
| CREDENTIALS       | 1   | User credentials are requested                                     |
| PKCS11            | 2   | PKCS#11/Smart card operation                                       |


Valid `ClientAttentionGroup` values are:

| Identifier        | ID  | Description                                                        |
|-------------------|:---:|--------------------------------------------------------------------|
| UNSET             | 0   | This is an invalid value, used for initialization only             |
| USER_PASSWORD     | 1   | Classic username/password authentication                           |
| HTTP_PROXY_CREDS  | 2   | Credentials for authenticating to the HTTP proxy                   |
| PK_PASSPHRASE     | 3   | Passphrase for the user's private key                              |
| CHALLENGE_STATIC  | 4   | Static challenge/response authentication, typically acquired before a connection starts |
| CHALLENGE_DYNAMIC | 5   | Dynamic challenge/response authentication, requested by the VPN server |
| PKCS11_SIGN       | 6   | PKCS#11 signature operation                                        |
| PKCS11_DECRYPT    | 7   | PKCS#11 decrypt operation                                          |
| OPEN_URL          | 8   | URL for web authentication                                         |

All of these references are declared in `src/dbus/constants.hpp`.


#### Arguments

| Direction | Name            | Type              | Description |
|-----------|-----------------|-------------------|-------------|
| Out       | type_group_list | array(uint, uint) | An array of tuples of `(ClientAttentionType, ClientAttentionGroup)` |



### Method: `net.openvpn.v3.backends.UserInputQueueCheck`

This is used to get the proper index values of information requests
the backend has asked for and which is not yet satsified.  The index
list is tied to a specific `(ClientAttentionType,
ClientAttentionGroup)` tuple.

#### Arguments

| Direction | Name    | Type        | Description                                    |
|-----------|---------|-------------|------------------------------------------------|
| In        | type    | uint        | `ClientAttentionType` reference to query for   |
| In        | group   | uint        | `ClientAttentionGroup` reference to query for  |
| Out       | indexes | array(uint) | An array of indexes which needs to be provided |


### Method: `net.openvpn.v3.backends.UserInputQueueFetch`

This method returns details about a specific information request from
the backend process.

#### Arguments

| Direction | Name         | Type    | Description                                                |
|-----------|--------------|---------|------------------------------------------------------------|
| In        | type         | uint    | `ClientAttentionType` reference to query for               |
| In        | group        | uint    | `ClientAttentionGroup` reference to query for              |
| In        | id           | uint    | Request ID to query for, provided by `UserInputQueueCheck` |
| Out       | type         | uint    | `ClientAttentionType` reference                            |
| Out       | group        | uint    | `ClientAttentionGroup` reference                           |
| Out       | id           | uint    | Request ID                                                 |
| Out       | name         | string  | An internal variable name                                  |
| Out       | description  | string  | A description to present to the front-end user             |
| Out       | hidden_input | boolean | If true, the user's input should be masked/hidden          |


### Method: `net.openvpn.v3.backends.UserInputProvide`

This method is used to return information from the front-end
application to the backend serivce.

#### Arguments

| Direction | Name         | Type    | Description                                                |
|-----------|--------------|---------|------------------------------------------------------------|
| In        | type         | uint    | `ClientAttentionType` reference to query for               |
| In        | group        | uint    | `ClientAttentionGroup` reference to query for              |
| In        | id           | uint    | Request ID to query for, provided by `UserInputQueueCheck` |
| In        | value        | string  | The front-end's response to the backend                    |


### Signal: `net.openvpn.v3.backends.StatusChange`

This signal is issued each time specific events occurs. They can both
be from the session object itself or StatusChange messages proxied
from the VPN client backend process.

#### Arguments

| Name        | Type   | Description                                                            |
|-------------|--------|------------------------------------------------------------------------|
| code_major  | uint   | Major status group classification.  Maps to enum class StatusMajor     |
| code_minor  | uint   | Minor status category classification within the status group. Maps to enum class StatusMinor  |
| message     | string | An optional string containing a more descriptive message of the signal |

See `src/dbus/constants.hpp` for more details on valid values.


### Signal: `net.openvpn.v3.backends.Log`

Whenever a specific session want to log something, it issues a Log
signal which carries a log group, log verbosity level and a string
with the log message itself.  The D-Bus path provided in the signal
points at the issuing VPN session.  See the separate [logging
documentation](dbus-logging.md) for details on this signal.

Beware that the back-end Log signalling differs slightly from the front-end
signalling, where the Log signal is extended to carry a session token as well.
This is due to the back-end client process cannot separate Log signals via the
D-Bus object path.


### Signal: `net.openvpn.v3.backends.AttentionRequired`

This signal is issued whenever the backend needs information, most
commonly form the front-end user interface. This can be used to get
user credentials or do PKCS#11/SmartCard operations, etc.

#### Arguments

| Name      | Type   | Description                                     |
|-----------|--------|-------------------------------------------------|
| type      | uint   | `ClientAttentionType` reference of the request  |
| group     | uint   | `ClientAttentionGroup` reference of the request |
| message   | string | A string containing a description of what kind of information being requested |


### Signal: `net.openvpn.v3.backends.RegistrationRequest`

This signal is sent once during the start-up of the backend VPN client
process initialization phase. The backend requires its
`net.openvpn.v3.backends.RegistrationConfirmation` method to be called
by the session manager after this signal have been sent to complete
the initialization.


#### Arguments

| Name      | Type   | Description                                     |
|-----------|--------|-------------------------------------------------|
| busname   | string | This contains this backend VPN clients process well-known busname. This is formatted as `net.openvpn.v3.backends.be${PID}` where `${PID`} references its own process ID |
| token     | string | Initial start-up token, used by the session manager to verify the VPN backend process relation to the session object |


### `Properties`
| Name          | Type             | Read/Write | Description                |
|---------------|------------------|:----------:|----------------------------|
| log_level     | uint             | read-write | Controls the log verbosity of messages intended to be proxied to the user front-end. **Note:** Not currently implemented |
| session_name  | string           | Read-only  | Session name generated by the OpenVPN 3 Core library after a successful connection has been established |
| statistics    | dictionary       | Read-only  | Contains tunnel statistics |
| status        | (uint, uint, string) | Read-only | Last issued StatusChange signal, as a tuple list (StatusMinor, StatusMajor, StatusDescription) |
| dco           | boolean          | read-write | Kernel based Data Channel Offload flag. Must be modified before calling Connect() to override the current setting. |
| device_path   | object path      | Read-only  | D-Bus object path to the net.openvpn.v3.netcfg device object related to this session |
| device_name   | string           | Read-only  | Virtual network interface name used by this session |


#### Dictionary: statistics

This is dictionary may have few or many fields, it depends on which
events have occurred by the OpenVPN 3 Core library for a specific
session.  The list below is not exhaustive and may change.

| Name               | Type   | Description                                         |
|--------------------|--------|-----------------------------------------------------|
| BYTES_IN           | uint64 | Number of bytes received on the TCP/UDP socket      |
| BYTES_OUT          | uint64 | Number of bytes sent from the TCP/UDP socket        |
| PACKETS_IN         | uint64 | Number of packets received on the TCP/UDP socket    |
| PACKETS_OUT        | uint64 | Number of packets sent from the TCP/UDP socket      |
| TUN_BYTES_IN       | uint64 | Number of bytes received on the TUN/TAP interface   |
| TUN_BYTES_OUT      | uint64 | Number of bytes sent from the TUN/TAP interface     |
| TUN_PACKETS_IN     | uint64 | Number of packets received on the TUN/TAP interface |
| TUN_PACKETS_OUT    | uint64 | Number of packets sent from the TUN/TAP interface   |
| NETWORK_SEND_ERROR | uint64 | Number of times there where issues sending packets  |
| KEEPALIVE_TIMEOUT  | uint64 | Number of times the tunnel keepalive restart was triggered |
| N_PAUSE            | uint64 | Number of times the tunnel was paused               |
| N_RECONNECT        | uint64 | Number of times the tunnel needed to do a reconnect |

