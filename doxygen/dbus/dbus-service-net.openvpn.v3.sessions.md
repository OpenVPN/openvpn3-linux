OpenVPN 3 D-Bus API: Session manager
====================================

The session manager keeps track of all the currently running VPN
tunnels. This is the primary front-end interface for managing VPN
tunnels. This manager will also proxy metod calls and signals from the
independent VPN backend client processes to listening user front-end
processes.


D-Bus destination: `net.openvpn.v3.sessions` \- Object path: `/net/openvpn/v3/sessions`
---------------------------------------------------------------------------------------

```
node /net/openvpn/v3/sessions {
  interface net.openvpn.v3.sessions {
    methods:
      NewTunnel(in  o config_path,
                out o session_path);
      FetchAvailableSessions(out ao paths);
      TransferOwnership(in  o path,
                        in  u new_owner_uid);
    signals:
      Log(u group,
          u level,
          s message);
    properties:
  };
};
```

### Method: `net.openvpn.v3.sessions.NewTunnel`

This starts a new VPN backend client process for a specific VPN
configuration profile. This does not start the connection, it just
starts a privileged client process and awaits further
instructions. When this method call returns with a session path, it
means the backend process have started.

#### Arguments

| Direction | Name         | Type        | Description                                                               |
|-----------|--------------|-------------|---------------------------------------------------------------------------|
| In        | config_path  | object path | A string containing the D-Bus object path of the VPN profile              |
| Out       | session_path | object path | A string containing a unique D-Bus object path to the created VPN session |


### Method: `net.openvpn3.v3.sessions.FetchAvailableSessions`

This method will return an array of object paths to session objects the
caller is granted access to.

#### Arguments
| Direction | Name        | Type         | Description                                            |
| Out       | paths       | object paths | An array of object paths to accessible session objects |


### Method: `net.openvpn3.v3.sessions.TransferOwnership`

This method transfers the ownership of a session to the given UID value.  This
feature is by design restricted to the root account only and is only expected
to be used by `openvpn3-autoload` and similar tools.

This method is also placed in the main session manager object and not the
session object itself by design, to emphasize this being a special case feature.
This also makes it easier to control this feature in the D-Bus policy in
addition to the hard-coded restriction in the session manager code.

#### Arguments
| Direction | Name          | Type         | Description                                            |
| In        | path          | object path  | Session object path where to modify the owner property |
| In        | new_owner_uid | unsigned int | UID value of the new session owner                     |


### Signal: `net.openvpn.v3.sessions.Log`

Whenever the session manager want to log something, it issues a Log
signal which carries a log group, log verbosity level and a string
with the log message itself. See the separate [logging
documentation](dbus-logging.md) for details on this signal.


D-Bus destination: `net.openvpn.v3.sessions` \- Object path: `/net/openvpn/v3/sessions/${UNIQUE_ID`}
----------------------------------------------------------------------------------------------------

```
node /net/openvpn/v3/sessions/${UNIQUE_ID} {
  interface net.openvpn.v3.sessions {
    methods:
      Connect();
      Pause(in  s reason);
      Resume();
      Restart();
      Disconnect();
      Ready();
      AccessGrant(in  u uid);
      AccessRevoke(in  u uid);
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
      AttentionRequired(u type,
                        u group,
                        s message);
      StatusChange(u code_major,
                   u code_minor,
                   s message);
      Log(u group,
          u level,
          s message);
    properties:
      readonly u owner;
      readonly t session_created;
      readonly au acl;
      readwrite b public_access;
      readonly s status;
      readonly a{sv} last_log;
      readonly a{sx} statistics;
      readonly o config_path;
      readonly u backend_pid;
      readwrite b restrict_log_access;
      readwrite b receive_log_events;
      readwrite u log_verbosity;
  };
};
```

### Method: `net.openvpn.v3.sessions.Ready`

This method is to check if the backend VPN client is ready to
connect. If it is ready, it will return immediately. If it is not, it
will return with a D-Bus error exception providing the reason it is
not ready. Most commonly it needs some input from the user, such as
user credentials or some challenge token not already provided in the
configuration.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.sessions.Connect`

This method starts the connection process. This requires that all
credentials needed before contacting the server have been provided. It
is always a good idea to first call the
`net.openvpn.v3.sessions.Ready` method first to ensure the backend is
ready to connect.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.sessions.Pause`

This method pauses an on-going VPN connection.

#### Arguments

| Direction | Name   | Type         | Description                                                                    |
|-----------|--------|--------------|--------------------------------------------------------------------------------|
| In        | reason | string       | A string used for log purposes primarily, describing why the tunnel was paused |


### Method: `net.openvpn.v3.sessions.Resume`

Resumes a paused VPN connection

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.sessions.Restart`

Completely disconnects and reconnects an active VPN connection

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.sessions.Disconnect`

Disconnects a VPN connection. This will shutdown and stop the VPN
backend process and the session object will be removed.

#### Arguments

(No arguments)


### Method: `net.openvpn.v3.sessions.AccessGrant`

By default, only the user ID (UID) who created the session have
access to it.  This method used to grant other users access to this
session.


#### Arguments

| Direction | Name | Type         | Description                                         |
|-----------|------|--------------|-----------------------------------------------------|
| In        | uid  | unsigned int | The UID to the user account which is granted access |


### Method: `net.openvpn.v3.sessions.AccessRevoke`

This revokes access to a session object for a specific user.
Please note that the owner (the user which initiated this session)
cannot have its access revoked.

#### Arguments

| Direction | Name | Type         | Description                                               |
|-----------|------|--------------|-----------------------------------------------------------|
| In        | uid  | unsigned int | The UID to the user account which gets the access revoked |



### Method: `net.openvpn.v3.sessions.UserInputQueueGetTypeGroup`

See the `net.openvpn.v3.backends.UserInputQueueGetTypeGroup` in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies this method call to the
backend process.


### Method: `net.openvpn.v3.sessions.UserInputQueueCheck`

See the `net.openvpn.v3.backends.UserInputQueueCheck` in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies this method call to the
backend process.


### Method: `net.openvpn.v3.sessions.UserInputQueueFetch`

See the `net.openvpn.v3.backends.UserInputQueueFetch` in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies this method call to the
backend process.


### Method: `net.openvpn.v3.sessions.UserInputProvide`

See the `net.openvpn.v3.backends.UserInputProvide` in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies this method call to the
backend process.


### Signal: `net.openvpn.v3.sessions.AttentionRequired`

See the `net.openvpn.v3.backends.AttentionRequired` entry in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies these signals from the
backend process to front-ends subscribing to this signal.


### Signal: `net.openvpn.v3.sessions.StatusChange`

See the `net.openvpn.v3.backends.StatusSignal` entry in
[`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies these signals from the
backend process to front-ends subscribing to this signal.


### Signal: `net.openvpn.v3.sessions.Log`

Whenever a specific session want to log something, it issues a Log
signal which carries a log group, log verbosity level and a string
with the log message itself.  The D-Bus path provided in the signal
points at the issuing VPN session.  See the separate [logging
documentation](dbus-logging.md) for details on this signal.


### `Properties`
| Name          | Type             | Read/Write | Description                                         |
|---------------|------------------|:----------:|-----------------------------------------------------|
| owner         | unsigned integer | Read-only  | The UID value of the user which did the import      |
| session_created| uint64          | Read-only  | Unix Epoc timestamp of when the session was created |
| acl           | array(integer)   | Read-only  | An array of UID values granted access               |
| public_access | boolean          | Read/Write | If set to true, access control is disabled. But only owner may change this property, modify the ACL or delete the configuration |
| status        | dictionary       | Read-only  | Contains the last processed StatusChange signal |
| last_log      | dictionary       | Read-only  | Contains the last Log signal proxied from the backend process |
| statistics    | dictionary       | Read-only  | Contains tunnel statistics |
| config_path   | object path      | Read-only  | D-Bus object path to the configuration profile used |
| backend_pid   | uint             | Read-only  | Process ID of the VPN backend client process |
| restrict_log_access | boolean    | Read-Write | If set to true, only the session owner can modify receive_log_events and log_verbosity, otherwise all granted users can access the log settings |
| receive_log_events | boolean     | Read-Write | If set to true, the session manager will proxy log events from the VPN backend process |
| log_verbosity | uint             | Read-Write | Defines the minimum log level Log signals should have to be sent |


#### Dictionary: status

This is essentially a saved StatusChange signal and contains the following references in its dictionary

| Name           | Type   | Description           |
|----------------|--------|-----------------------|
| major          | uint   | StatusMajor reference |
| minor          | uint   | StatusMinor reference |
| status_message | string | An optional string containing a more descriptive message of the signal |


#### Dictionary: last_log

This is essentially a saved Log signal and contains the following references in its dictionary

| Name         | Type   | Description           |
|--------------|--------|-----------------------|
| log_group    | uint   | LogGroup reference    |
| log_category | uint   | LogCategory reference |
| log_message  | string | The log message       |


#### Dictionary: statistics

See the properties section in [`net.openvpn.v3.backends`
client](dbus-service.net.openvpn.v3.client.md) documentation for
details.  The session manager just proxies the contents of the
`statistics` property from the backend process.

