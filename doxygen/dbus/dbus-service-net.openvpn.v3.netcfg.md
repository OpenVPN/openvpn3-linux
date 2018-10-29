OpenVPN 3 D-Bus API: Network configuration service
==================================================

This service is only accessible and used by the client service.

The interface provided here is similar to the
[VPNService interface of Android](https://developer.android.com/reference/android/net/VpnService)
but adapted for Linux and D-Bus.

When this backend process have been idle for a short time, it will
terminate itself automatically. It is only needed while a VPN connection
is active.

D-Bus destination: `net.openvpn.v3.netcfg` \- Object path: `/net/openvpn/v3/netcfg`
---------------------------------------------------------------------------------------
```
node /net/openvpn/v3/netcfg {
  interface net.openvpn.v3.netcfg {
    methods:
      CreateVirtualInterface(in  s dev_name,
                             out o device_path);
      FetchInterfaceList(out ao device_paths);
      ProtectSocket(in  s remote,
                    in  b ipv6,
                    out b succeded);
    signals:
      Log(u group,
          u level,
          s message);
    properties:
       readwrite u log_level;
  };
};
```


### Method: `net.openvpn.v3.backends.CreateVirtualInterface`

Create a virtual interface and return the object path of the new interface.

#### Arguments
| Direction | Name         | Type        | Description                                                      |
|-----------|--------------|-------------|------------------------------------------------------------------|
| In        | dev_name     | string      | A user friendly name for the device, will be part of device_path |
| Out       | device_path  | object path | A unique D-Bus object path for create device                     |

## Method: `net.openvpn.v3.configuration.FetchInterfaceList`

This method will return an array of object paths to virtual interfaces the
caller is granted access to.

#### Arguments
| Direction | Name        | Type         | Description                                                |
|-----------|-------------|--------------|------------------------------------------------------------|
| Out       | paths       | object paths | An array of object paths to accessible virtual interfaces  |


### Method: `net.openvpn.v3.backends.ProtectSocket`

This method is called by the service client to signal that a socket
needs to be `protected` from being routed over the VPN to avoid routing
loops. The method of how this is actually implemented can be controlled
by command line arguments to the netcfg service process.

#### Arguments

This method also

| Direction | Name         | Type        | Description                                                |
|-----------|--------------|-------------|------------------------------------------------------------|
| In        |              | fdlist      | File descriptor of the socket to protect [1]. Only the first provided fd is being processed. |
| In        | remote       | string      | The remote host this socket is connected to.               |
| In        | ipv6         | boolean     | The initial process ID (PID) of the VPN backend client.    |

[1] Unix file descriptors that are passed are not in the D-Bus method signature


### Signal: `net.openvpn.v3.sessions.Log`

Whenever the backend process starter needs to log something, it issues
a Log signal which carries a log group, log verbosity level and a
string with the log message itself. See the separate [logging
documentation](dbus-logging.md) for details on this signal.

### `Properties`
| Name          | Type             | Read/Write | Description                                         |
|---------------|------------------|:----------:|-----------------------------------------------------|
| log_level     | unsigned integer | read-write | Controls the log verbosity of messages intended to be proxied to the user front-end. **Note:** Not currently implemented |


D-Bus destination: `net.openvpn.v3.netcfg` \- Object path: `/net/openvpn/v3/netcfg/${UNIQUE_ID}`
--------------------------------------------------------------------------------------------------------------


```
node /net/openvpn/v3/netcfg/${UNIQUE_ID} {
interface net.openvpn.v3.netcfg {
    methods:
      AddIPAddress(in  s ip_address,
                   in  u prefix,
                   in  s gateway,
                   in  b ipv6);
      SetRemoteAddress(in  s ip_address,
                       in  b ipv6);
      AddNetworks(in  a(subb) networks);
      AddDNS(in  as server_list);
      AddDNSSearch(in  as domains);
      Establish();
      Disable();
      Destroy();
    signals:
      Log(u group,
          u level,
          s message);
      StateChange(u type,
                  s device,
                  s details);
    properties:
      readonly u owner;
      readonly au acl;
      readonly b active;
      readonly b modified;
      readonly as dns_search;
      readonly as dns_servers;
      readwrite u layer;
      readwrite u mtu;
      readwrite b reroute_ipv4;
      readwrite b reroute_ipv6;
      readwrite u txqueuelen;
  };
};
```

### Method: `net.openvpn.v3.backends.AddIPAddress`

Adds a new local IP Address to the VPN configuration of the virtual interface

#### Arguments
| Direction | Name         | Type        | Description                                                                  |
|-----------|--------------|-------------|------------------------------------------------------------------------------|
| In        | ip_address   | string      | The IP address in string representation (e.g. 198.51.100.12 or 2001:db8::23) |
| In        | prefix       | unsigned integer | The prefix length. (e.g. /24 or /64)                                         |
| In        | gateway      | string      | The IP address in string representation of the remote gateway inside the VPN |
| In        | ipv6         | ipv6        | Is the new IP address IPv6 or IPv4                                           |

### Method: `net.openvpn.v3.backends.SetRemoteAddress`

Set the remote address of the VPN server. This is the address the VPN
uses to connect to VPN server. This is used when creating direct routes
to the VPN server to avoid routing loops (redirect gateway option).

#### Arguments
| Direction | Name         | Type        | Description                                                                  |
|-----------|--------------|-------------|------------------------------------------------------------------------------|
| In        | ip_address   | string      | The IP address in string representation (e.g. 198.51.100.12 or 2001:db8::23) |
| In        | ipv6         | ipv6        | Is the IP address IPv6 or IPv4                                               |


### Method: `net.openvpn.v3.backends.AddNetworks`

Specifies a array of networks that should be either routed over the VPN or
explicitly not routed over the VPN. Conflicts between included and excluded
are resolved in the usual longest-prefix matching fashion.

#### Arguments
| Direction | Name         | Type        | Description                                                                  |
|-----------|--------------|-------------|------------------------------------------------------------------------------|
| In        | networks     | a(subb)     | An array of networks                                                         | 

A network is specified in the following way:

 | Name         | Type        | Description                                                                  |
 |--------------|-------------|------------------------------------------------------------------------------|
 | ip_address   | string      | The network IP address (the first IP in the network)                         |
 | prefix       | unsigned integer | The prefix of the network (e.g. /24 or /64)                                  |
 | ipv6         | boolean     | Is this a IPv6 or IPv4 network specification                                 |
 | exclude      | boolean     | If true, exclude (do not route) otherwise include (do route) this network over the VPN |

### Method: `net.openvpn.v3.backends.AddDNS`

Specifies a array of DNS server addresses that should be added to the list of DNS
server of the virtual interface.

#### Arguments
| Direction | Name         | Type              | Description                                              |
|-----------|--------------|-------------------|----------------------------------------------------------|
| In        | server_list  | array of strings  | An array of DNS server IP addresses.                     |

### Method: `net.openvpn.v3.backends.AddDNSSearch`

Specifies a array of DNS search domains that should be added to the list
of DNS search to the network.

#### Arguments
| Direction | Name         | Type              | Description                                              |
|-----------|--------------|-------------------|----------------------------------------------------------|
| In        | domains      | array of strings  | An array of DNS domains                                  |

### Method: `net.openvpn.v3.backends.Establish`

Uses all the information provided to the interface to setup a tun device
and set routes, DNS and interface accordingly. The resulting tun device
is returned to the caller.

#### Arguments
| Direction | Name         | Type              | Description                                                |
|-----------|--------------|-------------------|------------------------------------------------------------|
| Out       |              | fdlist            | The file descriptor corresponding to the new tun device [1]|

[1] Unix file descriptors that are passed are not in the D-Bus method signature.

### Method: `net.openvpn.v3.backends.Disable`

Indicates that the interface is temporarily not used by the VPN service.
E.g. that the VPN connection is disconnected and currently reconnecting.
**Note:** This is currently not implemented.

### Method: `net.openvpn.v3.backends.Destroy`

Removes the virtual interface and undoes the configuration (routes, DNS, tun device configuration). The
calling application must close the tun device own its own.

### `Properties`
| Name          | Type             | Read/Write | Description                                                   |
|---------------|------------------|:----------:|---------------------------------------------------------------|
| owner         | unsigned integer | Read-only  | The UID value of the user which did the import                |
| acl           | array(integer)   | Read-only  | An array of UID values granted access                         |
| log_level     | unsigned integer | read-write | Controls the log verbosity of messages intended to be proxied to the user front-end. **Note:** Not currently implemented |
| active        | boolean          | Read-only  | If the VPN is active (Establish has been successfully called) |
| dns_servers   | array of strings | Read-only  | Return the array of DNS servers                               |
| dns_search    | array of strings | Read-only  | Return the array of DNS search domains                        |
| layer         | unsigned integer             | Read-write | Sets the layer for the VPN to use, 3 for IP (tun device). Setting to 2 (tap device) is currently not implemented |
| mtu           | unsigned integer | Read-write | Sets the MTU for the tun device. Default is 1500              |
| reroute_ipv4  | boolean          | Read-write | Setting this to true, tells the service that the default route should be pointed to the VPN and that mechanism to avoid routing loops should be taken |
| reroute_ipv6  | boolean          | Read-Write | As reroute_ipv4 but for IPv6                                  |
| txqueuelen    | unsigned integer | Read-Write | Set the TX queue length of the tun device. If set to 0 or unset, the default from the operating system is used instead |
