OpenVPN 3 D-Bus API: Backend process starter
=============================================

This service is only accessible and used by the session manager. The
backend VPN client processes needs to start with root privileges to be
able to setup and configure TUN/TAP adapters and modify the routing
table. This backend process starter and the backend VPN client
processes are the only ones running with root privileges.

The session manager issues a call to this backend process starter to
start a new backend VPN client process. D-Bus itself takes care of
starting the process with root privileges upon this call.

When this backend process have been idle for a short time, it will
terminate itself automatically. It is only needed to start the backend
VPN client process.


D-Bus destination: `net.openvpn.v3.backends` \- Object path: `/net/openvpn/v3/backends`
---------------------------------------------------------------------------------------

node /net/openvpn/v3/backends {
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


### Method: `net.openvpn.v3.backends.StartClient`

This method is called by the session manager which triggers the
backend process starter to start a new `openvpn3-service-client`
process.

#### Arguments

| Direction | Name         | Type        | Description                                                |
|-----------|--------------|-------------|------------------------------------------------------------|
| In        | token        | string      | A unique token string created by the session manager. *1   |
| Out       | pid          | uint        | The initial process ID (PID) of the VPN backend client. *2 |

*1 This token is used by the VPN backend process to identify itself
 for a specific session object within the sessin manager.

*2 This initial PID will change, as the VPN backend process will do a
 double fork() to become its own process session leader.


### Signal: `net.openvpn.v3.sessions.Log`

Whenever the backend process starter needs to log something, it issues
a Log signal which carries a log group, log verbosity level and a
string with the log message itself. See the separate [logging
documentation](dbus-logging.md) for details on this signal.

