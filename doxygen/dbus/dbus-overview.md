OpenVPN 3 Linux Client - D-Bus overview
=======================================

The OpenVPN 3 client for Linux will be very different from what most
users know from OpenVPN 2. The design goals for the Linux client is to
utilize the facilities modern Linux distributions builds upon. The
reason for this is to have a better privilege separation between the
various operations OpenVPN needs to do and the users starting VPN
tunnels. In Linux, D-Bus have gained a lot of traction and being
worked actively on, and there are works in the pipe to facilitate some
of the message processing into kernel space as well (aka Bus1).

If you have no prior experience with D-Bus, it is recommended to first
read [D-Bus primer - Understanding the
bus](dbus-primer.md).


D-Bus policies and user groups
------------------------------

What might be surprising is that even the root user on D-Bus is
unprivileged when it comes to do operations on a D-Bus service. So the
whole OpenVPN 3 design considers this and aims to provide the bare
minimum of privileges to the services the OpenVPN 3 services provides
over the D-Bus. The current design splits users into three distinct
groups for the D-Bus policies implemented

*   Default: That is the default policy for all users, root
    included. The operations allowed over D-Bus will typically be
    processing of VPN configuration profiles and starting and
    management of VPN tunnels on the system
*   openvpn: The OpenVPN user account will have some restricted
    privileges to manage the OpenVPN services (typically backend VPN
    client processes)
*   root: The network configuration service (openvpn3-service-netcfg)
    needs to be started as root, but it will drop root privileges as soon
    as the needed capabilities have been set up.  This service will at
    least require `CAP_NET_ADMIN` but might add more depending on the
    DNS configuration backend it is configured to use.

The default OpenVPN 3 D-Bus policy will be installed in
`/etc/dbus-1/system.d/net.openvpn.v3.conf`, which is a location the
D-Bus daemon is configured to use by default.

It is also possible to define much more fine grained policies which is
managed by `polkit` (former Policy Kit), this is currently not
implemented but is planned to be used in the future. The default
actions are defined in XML files and the more fine grained policy
processing is written JavaScript. These files are located in
`/etc/polkit-1` and `/usr/share/polkit-1`.


OpenVPN 3 D-Bus services
------------------------

The OpenVPN 3 Client is split into several individual processes,
called services in the D-Bus terminology. These processes runs in the
background as daemons and only provides a D-Bus interface. Most of
them are automatically started by the D-Bus main daemon when a
front-end tries to establish contact with a service. If any of these
object run idle for a certain time and does not carry any
non-persistent data, they will automatically shutdown


|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.log`](dbus-service-net.openvpn.v3.log.md) |
| Running as   | openvpn                                 |
| Process name | openvpn3-service-logger                 |
| Started by   | Front-end clients and backend processes |
|   | Log service which receives all `Log` events from all of the OpenVPN 3 Linux services and directs them to log files or console logging.  See also the [OpenVPN 3 Linux Client: Logging](dbus-logging.md) document for more details on the logging. |
|              |                                         |


|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.configuration`](dbus-service-net.openvpn.v3.configuration.md) |
| Running as   | openvpn                                 |
| Process name | openvpn3-service-configmgr              |
| Started by   | Front-end clients and backend processes |
|   | Stores VPN configuration profiles in a format used by the OpenVPN 3 Core library, as well as providing an API to query and modify configuration options. |
|              |                                         |


|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| Running as   | openvpn                                 |
| Process name | openvpn3-service-sessionmgr             |
| Started by   | front-end clients  |
|   |  This service manages all VPN profiles being set up and throughout the whole life cycle until the VPN tunnel is disconnected. |
|              |                                         |

|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.netcfg`](dbus-service-net.openvpn.v3.netcfg.md) |
| Running as   | openvpn                                    |
| Process name | openvpn3-service-netcfg                 |
| Started by   | net.openvpn.v3.backends                 |
|   | This is the process which is responsible for setting up the priviliged network configuration for the openvpn session client. It allows the session client to run unpriviledges and also provides a generic interface to open a tun device and configure the VPN configuration of (IP, routes, DNS).  This process must be started as root. |
|              |                                         |

|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.backends`](dbus-service-net.openvpn.v3.backends.md) |
| Running as   | openvpn                                 |
| Process name | openvpn3-service-backendstart              |
| Started by   |  net.openvpn.v3.sessions (as the openvpn user) |
|   |  This service provides an interface for the session manager to start a the backend VPN client process |
|              |                                         |


|              |                                         |
|-------------:|-----------------------------------------|
| Service      | [`net.openvpn.v3.backends.be${PID}`](dbus-service-net.openvpn.v3.client.md) |
| Running as   | openvpn                                 |
| Process name | openvpn3-service-client                 |
| Started by   | net.openvpn.v3.backends                 |
|   | This is the process which is responsible for a single VPN tunnel. This process implements the OpenVPN 3 Core client, connects to remote servers and allows itself to be managed via the session manager. Each process will use its own unique service name, where the PID of the process is included in the service name. |
|              |                                         |



In addition to these services, there needs to be a front-end
application which interacts with these services on behalf of a user.


D-Bus services and auto-start configuration
-------------------------------------------

The OpenVPN 3 Linux client will auto-start the various D-Bus services
it depends on; this is a feature provided by the D-Bus message
daemon. This means that if no OpenVPN clients have been started, the
OpenVPN 3 client is not consuming any resources. Once a connection is
prepared and setup, the various backend processes are automatically
started with the right properties and privileges. Those processes
running idle without anything to do for a while will automatically
shutdown again by itself. This way the Linux client's backend
processes are a zero-footprint implementation when it is not being
used.

The auto-start profiles can be found in the
`/usr/share/dbus-1/system-services/net.openvpn.v3.*.service` files.


Typical process of starting a VPN tunnel
----------------------------------------

The steps needed to establish a tunnel needs to do the following
steps.

### Initialize / provide configuration file

|                   |                                         |
|------------------:|-----------------------------------------|
| Step              | Import configuration file               |
| D-Bus destination | [`net.openvpn.v3.configuration`](dbus-service-net.openvpn.v3.configuration.md)|
| D-Bus path        | `/net/openvpn/v3/configuration` |
| D-Bus method      | `net.openvpn.v3.configuration.Import` |
| Returns           | Unique path to a new configuration object |
|  |  |

This step imports a standard OpenVPN configuration file into the
configuration manager. The configuration file must include all
external files embedded into the data being imported.

### Prepare tunnel

|                   |                                         |
|------------------:|-----------------------------------------|
| Step              | Create a VPN session object and prepare for connecting |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | `/net/openvpn/v3/sessions` |
| D-Bus method      | `net.openvpn.v3.sessions.NewTunnel` |
| Returns           | Unique path to a new session object |
|  |  |

This D-Bus method call needs to provide the unique path to a
configuration object, provided by the Import call in the previous
step. This call will return a unique object path to this particular
VPN session.

If information from the user is required, the session manager will
issue a `AttentionRequired` signal which describes what it
requires. It is the front-end application's responsibility to act upon
these signals. Alternatively, calling the
net.openvpn.v3.sessions.Ready method can be used; see below for more
details


|                   |                                         |
|------------------:|-----------------------------------------|
| Step              | Check if tunnel is ready to connect     |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session       |
| D-Bus method      | `net.openvpn.v3.sessions.Ready`          |
| Returns           | (nothing)                               |
|  |  |

This method call must be called to ensure the backend VPN process is
ready to connect. It will not return anything if it is ready to
connect. Otherwise it will return an exception with more details.


### Provide user credentials

If the Ready call results in an exception requiring user credentials
or the front-end receives an `AttentionRequired` signal, it is needed
to retrieve information about what the backend requires. All
requirements are put into a User Input Queue.

|                   |                                              |
|------------------:|----------------------------------------------|
| Step              | Check what kind of credentials are requested |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session            |
| D-Bus method      | `net.openvpn.v3.sessions.UserInputQueueGetTypeGroup` |
| Returns           | An array of `ClientAttentionType` and `ClientAttentionGroup` tuples |
|  |  |

Calling `UserInputQueueGetTypeGroup` will return an array of types
(ClientAttentionType) and group (ClientAttentionGroup) references
which have requirements the front-end needs to satisfy.


|                   |                                              |
|------------------:|----------------------------------------------|
| Step              | Check what kind of credentials are requested |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session            |
| D-Bus method      | `net.openvpn.v3.sessions.UserInputQueueCheck` |
| Returns           |                                |
|  |  |

The `UserInputQueueCheck` needs to be called with the
ClientAttentionType and ClientAttentionTypeGroup, this returns a list
of IDs which needs to be fulfilled.

|                   |                                          |
|------------------:|------------------------------------------|
| Step              | Retrieve information about a requirement |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session        |
| D-Bus method      | `net.openvpn.v3.sessions.UserInputQueueFetch`|
| Returns           | (nothing)                                |
|  |  |


For each of these IDs, the front-end is required to call
`UserInputQueueFetch` to get the needed information to provide to the
front-end application.  This application will then need to consider if
the end-user will be presented with some prompts, or if it has direct
access to the needed information through other channels.

|                   |                                              |
|------------------:|----------------------------------------------|
| Step              | Provide requested information to the backend |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session            |
| D-Bus method      | `net.openvpn.v3.sessions.UserInputProvide`   |
| Returns           | (nothing)                                    |
|  |  |

This method needs the ClientAttentionType, ClientAttentionGroup, ID
and a string containing the information the backend requested.


|                   |                                         |
|------------------:|-----------------------------------------|
| Step              | Check if tunnel is ready to connect     |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session       |
| D-Bus method      | net.openvpn.v3.sessions.Ready`          |
| Returns           | (nothing)                               |
|  |  |


#### `AttentionRequired` signal vs `UserInputQueueCheck`

The `AttentionRequired` signal only provides a signal that some interaction is needed by the front-end process; think of it as a PUSH notification. The `UserInputQueueGetTypeGroup` and `UserInputQueueCheck` methods can be called at any time to check if some information is missing; think of it as a POLL operation.


### Activate a VPN tunnel

|                   |                                         |
|------------------:|-----------------------------------------|
| Step              | Start connecting to the remote server   |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session       |
| D-Bus method      | net.openvpn.v3.sessions.Connect`        |
| Returns           | (nothing)                               |
|  |  |

This starts the VPN tunnel and it will try to connect to the remote
server. The call will return immediately and the front-end user
application needs to watch the `StatusChange` and `AttentionRequired`
signals to follow the real-time situation of the VPN connection. As
with the tunnel preparation, the front-end application must respond to
any `AttentionRequired` signal by calling the
`net.openvpn.v3.sessions.``UserInputProvide` method.

In situations where the server requires more authentication
credentials (dynamic challenge), the backend client will disconnect
from the server and issue the `AttentionRequired` signal.  By calling
the `Ready` method, it will also throw an approriate exception if the
authentication was not satisfied.


### Pause, resume and restart an existing VPN tunnel

|                   |                                          |
|------------------:|------------------------------------------|
| Step              | Temporarily suspend an active connection |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session        |
| D-Bus method      | net.openvpn.v3.sessions.Pause`           |
| Returns           | (nothing)                                |
|  |  |


|                   |                                          |
|------------------:|------------------------------------------|
| Step              | Resume an already suspend an connection  |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session        |
| D-Bus method      | net.openvpn.v3.sessions.Resume`          |
| Returns           | (nothing)                                |
|  |  |


|                   |                                          |
|------------------:|------------------------------------------|
| Step              | Restart an active connection             |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session        |
| D-Bus method      | net.openvpn.v3.sessions.Restart`         |
| Returns           | (nothing)                                |
|  |  |

This makes the VPN connection disconnect and then reconnect instantly.


### Disconnect and stop the tunnel

|                   |                                            |
|------------------:|--------------------------------------------|
| Step              | Stops and disconnects a session            |
| D-Bus destination | [`net.openvpn.v3.sessions`](dbus-service-net.openvpn.v3.sessions.md) |
| D-Bus path        | Unique path to a specific session          |
| D-Bus method      | net.openvpn.v3.sessions.Disconnect`        |
| Returns           | (nothing)                                  |
|  |  |

This disconnects the VPN client from the server and shuts down the
background VPN client process. This will also remove the D-Bus session
object and the unique path to this session object will no longer be
valid.

This is also used to shutdown the VPN client process, regardless if
the VPN tunnel is active or not.

