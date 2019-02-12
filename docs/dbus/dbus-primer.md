D-Bus Primer - Understanding the bus
====================================

D-Bus is first and foremost an Inter Process Communication (IPC) solution. But
it does a bit more than just passing a blob of information from one process to
another one. D-Bus is object oriented, where each object can have a set of
Methods, Properties and Signals. All the data passed through D-Bus is also
required to use the data types provided by D-Bus. This enforces a fairly
strict and predictable type checking, which means the application implementing
D-Bus will not need to care about parsing message protocols.

The D-Bus design is built around a server/client approach, where the server
side is most commonly referred to as a D-Bus service. And the client side
uses, what is often called, a proxy to do operations on D-Bus objects provided
by a specific D-Bus service. Operations can be calling methods or reading or
writing object properties. D-Bus object are referenced through D-Bus
paths. And a D-Bus service is accessed through a destination reference, which
is a bus name and a bus type indicator.

Bus types
---------

D-Bus also operates with several types of buses. The most commonly used
implementations use either the _system bus_ or the _session bus_. The system
bus is by default fairly strictly controlled and locked down. Users can only
access services on the system bus if the policy for that service allows
it. The session bus is only reachable by a single user, and most commonly also
only in a specific sessions. The session bus is also not that strict
configured, as its availability is much more reduced. For example if you are
logged in through a graphical desktop login, you most likely have a session
bus running for that login. If you at the same time from another computer log
in via SSH to the same user account on the same graphical desktop host, that
SSH login will not have access to the session bus which the graphical desktop
login uses - despite the username and host being identical.

Bus names
---------

Each process implementing a D-Bus is required to have a unique bus name,
regardless if it has the role of a server or client. This is typically using
the `:X.Y` notation. Also beware that this unique bus name is provided by the
D-Bus messaging daemon which is most likely running on your system
already. That means it is not a consistent value. In addition a service can
own a more generic and human readable service name, most commonly called a
well-known bus name. Such a well-known name can for example be
`net.openvpn.v3.configuration`. This is used for the server side
implementations, to have a predictable destination reference from the client
side. The implementation of the service instructs the D-Bus message daemon
what should happen if there is conflicting well-known bus names; if new
services should be denied, if existing services should loose the ownership to
the well-known bus name or if exiting services should just ignore it and
continue to run only to be reachable via the unique bus name. But it is also
important to beware that many of the responses received may very well return
the unique bus name and not the well-known bus name you used when connecting
to a D-Bus service.

Getting unto the bus
--------------------

To reach a D-Bus object, you need to first connect to either the system or
session bus. Then you need to get in touch with the service where it is most
common to use the well-known bus name. A well-known bus name is basically a
human readable string name, organized as a reversed domain name. For the
OpenVPN 3 Linux client we will use the base domain of `net.openvpn.v3`. When
connecting to a specific service, the well-known name is referred to as the
_destination_. Within that service you will have one or more objects
available. Using the proper object path, you often have a selection of
interfaces, where each interface have their own set of Methods, Signals and
Properties.

D-Bus Methods are functions you can call, which will be executed by the D-Bus
service. D-Bus properties are variables owned by an object which you can read
or write to, depending on their attributes. And D-Bus signals are events
happening inside a D-Bus service which it can use to get some attention to
changes. To receive such signals, you need to subscribe to them first.

Lets have a look at a simpler D-Bus service. We will here look at the service
used to start OpenVPN 3 VPN backend client processes. We will not look into
the service itself now, but focus on how to talk to this service.

```
$ gdbus introspect --system --dest net.openvpn.v3.backends --object-path /net/openvpn/v3/backends
node /net/openvpn/v3/backends {
  interface org.freedesktop.DBus.Properties {
    methods:
      Get(in  s interface_name,
          in  s property_name,
          out v value);
      GetAll(in  s interface_name,
             out a{sv} properties);
      Set(in  s interface_name,
          in  s property_name,
          in  v value);
    signals:
      PropertiesChanged(s interface_name,
                        a{sv} changed_properties,
                        as invalidated_properties);
    properties:
  };
  interface org.freedesktop.DBus.Introspectable {
    methods:
      Introspect(out s xml_data);
    signals:
    properties:
  };
  interface org.freedesktop.DBus.Peer {
    methods:
      Ping();
      GetMachineId(out s machine_uuid);
    signals:
    properties:
  };
  interface net.openvpn.v3.backends {
    methods:
      StartClient(in  s token,
                  out u pid);
    signals:
      Log(u group,
          u level,
          s message);
    properties:
  };
};
$
```

The command line used here, uses the D-Bus introspection feature which most
services provides. It gives an idea of what is available and the required
API. We ask for the introspection data of a service on the system bus (The
`--system` argument). Further, we connect to the `net.openvpn.v3.backend`
service and ask for the introspection data of the object located under
`/net/openvpn/v3/backends`.

The result provides a fairly comprehensive list which includes four different interfaces:

*   `org.freedesktop.DBus.Properties`
*   `org.freedesktop.DBus.Introspectable`
*   `org.freedesktop.DBus.Peer`
*   `net.openvpn.v3.backends`

In this section, we will only look at the last interface, `net.openvpn.v3.backends`.

This interface declares one method (`StartClient`) and a one signal
(`Log`). The method declares that it takes one input argument (`in s token`)
and provides one output argument (`out u pid`). Also note that D-Bus methods
may be declared to return several variables at the same time. And when the
proxy code parses the response, it may use the variable names as indicated.

The '`s`' reference in the variable declaration indicates that the data type
is a 'string'. The return variable is named pid and is an unsigned integer. So
to call the `StartClient` method, it is needed to provide a single string
containing a token value and on success it will return with a 'pid' variable
containing an unsigned integer. You can read more about the various data types
D-Bus supports here:
[https://dbus.freedesktop.org/doc/dbus-specification.html#type-system](https://dbus.freedesktop.org/doc/dbus-specification.html#type-system)

The `Log` signal is pretty much similar. The difference is that you don't call
signals, but subscribe to them. Whenever this D-Bus service issues the `Log`
signal, you will be provided with three variables in your signal handler code:
Two unsigned integers (`group` and `level`) as well as a string
(`message`). You will not get any heads up that a signal is being sent,
neither can you tell the D-Bus service to "send me all the unprocessed
signals". They happen asynchronously, and if you don't pay attention to it,
you will have lost the signal. So it can be fragile if not implemented
correctly.

Interacting with a D-Bus service
--------------------------------

Lets take a more advanced example, where we import a simple configuration file
to the OpenVPN 3 D-Bus Configuration Manager. First we will do an
introspection to see the API being provided.

```
$ gdbus introspect --system --dest net.openvpn.v3.configuration --object-path /net/openvpn/v3/configuration
node /net/openvpn/v3/configuration {

   /\* Removed the org.freedesktop.DBus.* interfaces for clarity */

  interface net.openvpn.v3.configuration {
    methods:
      Import(in  s name,
             in  s config_str,
             in  b single_use,
             in  b persistent,
             out o config_path);
    signals:
      Log(u group,
          u level,
          s message);
    properties:
  };
};
$
```

The configuration manager have an `Import` method which we will need to
use. Lets write a simple Python script to interact with this service and
import a simple configuration file. The important detail is that the `Import`
method takes four arguments, two strings and two boolean values and it will
return an object path - which is essentially a string with quite strict checks
on its content. See the
[net.openvpn.v3.configuration](dbus-api-net.openvpn.v3.configuration.md)
D-Bus API specification for details on these variables.

```python
import dbus

# Get a connection to the system bus
bus = dbus.SystemBus()

# Retrieve the main configuration manager object.
# We provide the service name and the initial object path as arguments.
# Those need to correspond to what we saw in the introspection.
manager_object = bus.get_object('net.openvpn.v3.configuration',    # Well-known bus name
                                '/net/openvpn/v3/configuration')   # Object path

# Retrieve access to the proper interface in the object
config_interface = dbus.Interface(manager_object,
                                  dbus_interface='net.openvpn.v3.configuration')

# Here is our super simple configuration file (not valid by the way, as it lacks --ca)
config_to_import = """
remote vpnserver.example.org
port 30001
proto udp
client-cert-not-required
auth-user-pass
"""

# Import the configuration, and we're given back an object path
config_path = config_interface.Import("Test config",     # name
                                      config_to_import,  # config_str
                                      False,             # single_use
                                      False)             # persistent
print("Configuration path: " + config_path)
```

When running this little Python script, the result will be something like
this:

```
Configuration path: /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82
```

This is essentially a new D-Bus object managed by the
`net.openvpn.v3.configuration` service. When the process providing this object
stops, this object is also not available any more. And to access this new
object, we need to use the path returned by the `Import` method. So lets
introspect that object as well.

```
$ gdbus introspect --system --dest net.openvpn.v3.configuration --object-path /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82
node /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 {

   /\* Removed the org.freedesktop.DBus.* interfaces for clarity */

  interface net.openvpn.v3.configuration {
    methods:
      Fetch(out s config);
      FetchJSON(out s config_json);
      SetOption(in  s option,
                in  s value);
      Seal();
      Remove();
    signals:
    properties:
      readonly s name = 'Test config';
      readonly b valid = true;
      readonly b readonly = false;
      readonly b single_use = false;
      readonly b persistent = false;
  };
};
$
```
And we can recognise several of these properties, as they were provided during
the `Import` method call. But we also have a different set of methods
available too. We can now use a simple `dbus-send` command line to call thee
`FetchJSON` method. Notice that when calling methods from the command line, we
need to use the interface name as well as the method name.


```
$ dbus-send --system --print-reply=literal --dest=net.openvpn.v3.configuration \
            /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 \
            net.openvpn.v3.configuration.FetchJSON
   {
    "auth-user-pass" : "",
    "client-cert-not-required" : "",
    "port" : "30001",
    "proto" : "udp",
    "remote" : "devtest1.openvpn.in"
}
$
```

It is also possible to use other D-Bus clients as well, to achieve the same
goal. But the output being presented can often be different. So for reference,
here is how to do the same call with both gdbus and qdbus (from Qt)

```
$ gdbus call --system --dest net.openvpn.v3.configuration \
             --object-path /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 \
             --method net.openvpn.v3.configuration.FetchJSON
('{\\n\\t"auth-user-pass" : "",\\n\\t"client-cert-not-required" : "",\\n\\t"port" : "30001",\\n\\t"proto" : "udp",\\n\\t"remote" : "devtest1.openvpn.in"\\n}',)
$ qdbus --system net.openvpn.v3.configuration \
                 /net/openvpn/v3/configuration/23165540x6646x42bexb530x464bb8e2df67 \
                 net.openvpn.v3.configuration.FetchJSON
{
	"auth-user-pass" : "",
	"client-cert-not-required" : "",
	"port" : "30001",
	"proto" : "udp",
	"remote" : "devtest1.openvpn.in"
}
$
```


### Accessing object properties

Accessing properties in D-Bus objects is a bit more tricky. Here it will be
demonstrated how to reach the `name` property in the configuration object we
have already available.

It is needed to use a specific interface within D-Bus framework to access
properties. These methods are located inside the
`org.freedesktop.DBus.Properties` interface in each available object. First,
lets look closer at that interface, again by using the introspection
possibility.

```
$ gdbus introspect --system --dest net.openvpn.v3.configuration --object-path /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82
node /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 {
  interface org.freedesktop.DBus.Properties {
    methods:
      Get(in  s interface_name,
          in  s property_name,
          out v value);
      GetAll(in  s interface_name,
             out a{sv} properties);
      Set(in  s interface_name,
          in  s property_name,
          in  v value);
    signals:
      PropertiesChanged(s interface_name,
                        a{sv} changed_properties,
                        as invalidated_properties);
    properties:
  };

  /\* Removed the rest, for clarity */
};
```

The method we are interested in is the `Get` method in the
`org.freedesktop.DBus.Properties` interface. This method requires two string
arguments, the first one is the _interface_ carrying the property we want to
read. The second argument is the _name_ _of the property_ we want to access.

```
$ dbus-send --system --print-reply --dest=net.openvpn.v3.configuration         \
            /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 \
            org.freedesktop.DBus.Properties.Get                                \
            string:'net.openvpn.v3.configuration' string:'name'
method return sender=:1.2920 -> dest=:1.3560 reply_serial=2
   variant       string "Test config"
$
```

The arguments we send to the `Get` method needs to be typed, so `dbus-send`
have its own syntax to handle that from the command line. Other programming
languages have their own ways of handling types. Python will for example
mostly do that automatically for you.

And just for reference, using `gdbus` and `qdbus` to do achieve the same. As
you can see by these examples, the arguments are considered to be strings by
default using these tools.

```
$ gdbus call --system --dest net.openvpn.v3.configuration \
             --object-path /net/openvpn/v3/configuration/2e356d14x6d51x4d16xbaf3x3d758626fc82 \
             --method org.freedesktop.DBus.Properties.Get net.openvpn.v3.configuration name
(<'Test config'>,)
$ qdbus --system net.openvpn.v3.configuration \
                 /net/openvpn/v3/configuration/23165540x6646x42bexb530x464bb8e2df67 \
                 org.freedesktop.DBus.Properties.Get "net.openvpn.v3.configuration" name
Test config
$
```

Conclusion
----------

To access a D-Bus object, you need to:

*   Connect to the proper bus (system or session)
*   Know which destination service you are targeting
*   Know which object you want to work with
*   Know which interface within that particular object you want to access

With these four areas covered, you have direct access to all the available
methods and properties inside any object on the D-Bus.

### Challenges

1.  Implement retrieving an object property using Python or another language
with D-Bus bindings available
2.  Try changing the `name` property of the configuration profile object.
    *   What happens? Try to do introspection on both the main configuration
        manager object and this specific configuration profile object.

