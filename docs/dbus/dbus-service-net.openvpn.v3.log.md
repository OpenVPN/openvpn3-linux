OpenVPN 3 D-Bus API: Log service
================================

The `openvpn3-service-logger` program can be run in two modes, as a pure
signal consumer of broadcasted `Log` signals or as a log service where
Log signal producers must attach to the service first.

In signal consumer mode, the services sending log signals must broadcast
the Log signals to anyone and the D-Bus policy must allow these signals to
be received by the user running `openvpn3-service-logger`. In this mode the
program must be started with at least one of the `--config-manager`,
`--session-manager` or `--vpn-backend` arguments.  This activates the
subscription for broadcasted `Log` signals, but only for the selected
senders.

In service mode, `openvpn3-service-logger` must be started with `--service`.
This registers the `net.openvpn.v3.log` D-Bus service where Log signal
producers can attach their signals to.  This has the benefit of ensuring
only the log service receives the Log signals.  This log service will also
be automatically started when needed by the D-Bus daemon, if it is not
already running.

In either mode, `openvpn3-service-logger` targets `Log` signals. For
[`net.openvpn.v3.backend.be$PID`](docs/dbus/dbus-service-net.openvpn.v3.client.md)
services, also StatusChange events are processed.

The log service can either send Log events to a predefined log file (via
`--log-file`), to syslog (via `--syslog`) or to the console (default).


## Log forwarding

When a process wants to receive Log and StatusChange events for a running VPN
session, it can request those signals to be forwarded to itself by calling
[`net.openvpn.v3.sessions.LogForward()`](dbus-service-net.openvpn.v3.sessions.md)
with the `true` value.  This makes the session manager configure a log proxy
proxy object in the `net.openvpn.v3.log` service which will proxy Log and
StatusChange signals to the requester.  This forwarding is disabled by calling
the same `LogForward()` method with the `false` value.


## Runtime Configuration

When running with `--service`, there are a few tweakable knobs which can
be managed using the `openvpn3-admin log-service` command.  This command can
only be used by the `root` and `openvpn` user accounts.  If
`openvpn3-service-logger` is started with the `--state-dir` option, these
runtime settings are preserved and will be reloaded automatically from the
given directory.


# OpenVPN 3 Logger D-Bus objects

D-Bus destination: `net.openvpn.v3.log` - Object path: `/net/openvpn/v3/log`
----------------------------------------------------------------------------

```
node /net/openvpn/v3/log {
  interface net.openvpn.v3.log {
    methods:
      Attach(in  s interface);
      AssignSession(in  o session_path,
                    in  s interface);
      Detach(in  s interface);
      GetSubscriberList(out a(ssss) subscribers);
      ProxyLogEvents(in  s target_address,
                     in  o session_path,
                     out o proxy_path);
    signals:
    properties:
      readwrite u log_level = 4;
      readwrite b log_dbus_details = false;
      readwrite b timestamp = true;
      readonly u num_attached = 0;
  };
};
```

### Method: `net.openvpn.v3.log.Attach`

This makes the log service aware of a Log signal producer which it needs
to subscribe to.  At the same time, the Log signal producer will then
target these signals only to the `net.openvpn.v3.log` D-Bus service.


#### Arguments

| Direction | Name        | Type        | Description                                                           |
|-----------|-------------|-------------|-----------------------------------------------------------------------|
| In        | interface   | string      | String containing the service interface to subscribe to.  If a service sends `Log` signals with different signals, each of these interfaces must be Attached |


### Method: `net.openvpn.v3.log.AssignSession`

A `net.openvpn.v3.backend.be$PID` service can through this method add a
link between the Session D-Bus path to a specific VPN client service.  This
is required to have happened before the `net.openvpn.v3.log` service can do
a lookup from a session path to log events coming from a specific client backend.

##### Arguments

| Direction | Name         | Type        | Description                                                           |
|-----------|--------------|-------------|-----------------------------------------------------------------------|
| In        | session_path | string      | D-Bus Session Path to the session this client is responsible for      |
| In        | interface    | string      | String containing the client interface log events are related to.     |


### Method: `net.openvpn.v3.log.Detach`

This is the reverse operation of `Attach`, where the log service will
unsubscribe from a specific log producing sender.  This is important to
avoid resource leaking in the log service.  Attached subscriptions should
not hurt the performance if they never send signals, but it should be
avoided to have too many idling subscriptions.

#### Arguments

| Direction | Name        | Type        | Description                                                           |
|-----------|-------------|-------------|-----------------------------------------------------------------------|
| In        | interface   | string      | String containing the service interface to unsubscribe from.  If a service sends `Log` signals with different signals, each of these interfaces must be `Detached` |


### Method: `net.openvpn.v3.log.GetSubscriberList`

Retrieve a list of all subscriptions the log service is attached to.  The
entries listed here are services which have used the `Attach` method in this
service.  Services calling the `Detach` method will be unlisted.

#### Arguments
| Direction | Name        | Type        | Description                                                                |
|-----------|-------------|-------------|-----------------|
| out       | subscribers | array       | See note below  |

##### Arguments: subscribers

The result of the `GetSubscriberList` method call is an array of tuples, each
containing four strings.

|  Field  |  Description                                                              |
|---------|---------------------------------------------------------------------------|
|    0    |  String containing a `tag` value which is used in the logs                |
|    1    |  String containing the bus name the log service is attached to            |
|    2    |  String containing the D-Bus object interface the subscription is tied to |
|    3    |  String containing the D-Bus object path the subscription is tied to      |


### Method: `net.openvpn.v3.log.ProxyLogEvents`

This method is by design only available by the `openvpn` user, which the
[`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) service is running
under.  The Session Manager can call this method to setup a new log recipient for a
given VPN session.  In addition to Log events being forwarded, StatusChange signals are
also part of this feature.

#### Arguments
| Direction | Name           | Type        | Description                                                           |
|-----------|----------------|-------------|-----------------------------------------------------------------------|
| In        | target_address | string      | D-Bus unique bus name for the recipient of Log and StatusChange events|
| In        | session_path   | object path | D-Bus object path to the VPN session object                           |
| Out       | proxy_path     | object path | D-Bus object path to the Log Proxy object in the logger service       |


### `Properties`

| Name          | Type             | Read/Write | Description                                         |
|---------------|------------------|:----------:|-----------------------------------------------------|
| log_level     | unsigned integer | Read/Write | How verbose should the logging be.  See the table below for the mapping between log levels and Log Category the `Log` signal carries` |
| log_dbus_details | boolean       | Read/Write | Should each Log event being processed carry a meta data line before with details about the D-Bus sender of the `Log` signal? |
| timestamp     | boolean          | Read/Write | Should each log line be prefixed with a timestamp?  This is mostly controlling the output when file or console logging is used. For syslog, timestamps are handled by syslog and the log service will enforce this to be `true`. |
| num_attached  | unsigned integer | Read-only  | Number of attached subscriptions.  When no `openvpn3-service-*` programs are running, this should ideally be `0`. |


#### Log levels and Log Category mapping

Each `Log` signal which contains the log message, are tagged with a Log
Group and Log Category.  See the
[OpenVPN 3 Linux Client: Logging](dbus-logging.md) document for
details.  The `log_level` is used to filter out messages which is too
verbose, and the table below lists the correlation between them.  A log
level of 2 will include all Log Categories tagged with log level 2, 1 and 0.
The higher the log level value is, the more verbose the logging will be.
The only valid log level values are between 0 and 6.

| Log level | Log Category |
|-----------|--------------|
|  0        | FATAL        |
|  0        | CRITICAL     |
|  1        | ERROR        |
|  2        | WARNING      |
|  3        | INFO         |
|  4        | VERB1        |
|  5        | VERB2        |
|  6        | DEBUG        |



D-Bus destination: `net.openvpn.v3.log` - Object path: `/net/openvpn/v3/log/proxy/${UNIQUE_ID}`
-----------------------------------------------------------------------------------------------

This object is created by calling the `net.openvpn.v3.log.ProxyLogEvents`
method.  This object contains information about each D-Bus client which will
retrieve `Log` and `StatusChange` signals sent by the VPN client session.

```
  interface net.openvpn.v3.log {
    methods:
      Remove();
    signals:
    properties:
      readwrite u log_level;
      readonly s session_path;
      readonly s target;
  };
```


### Method: `net.openvpn.v3.log.Remove`

By calling this method, the Log and StatusChange forwarding will be stopped.
This will also remove this D-Bus object.


### `Properties`
| Name          | Type             | Read/Write | Description                                           |
|---------------|------------------|:----------:|-------------------------------------------------------|
| log_level     | unsigned int     | Read/Write | Verbosity level for log events to this recipient      |
| session_path  | object path      | Read-only  | D-Bus object path to the VPN session                  |
| target        | string           | Read-only  | D-Bus unique bus name to the recipient (D-Bus client) |
