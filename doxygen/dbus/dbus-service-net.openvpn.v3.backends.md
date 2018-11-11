OpenVPN 3 D-Bus API: Backend process starter
=============================================

This service is only accessible for and used by the session manager and it uses
this backend-start to provide the needed information for the backend VPN
client process to register properly with the session manager and to get
information about the configuration profile to retrieve from the configuration
manager.

The session manager issues a call to this backend starter to
start a new backend VPN client process and the session manager will start
one backend VPN client process per running VPN session.

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

