OpenVPN 3 D-Bus: Logging
========================

Logging can happen over D-Bus log signals and is most commonly used to
provide information which may be presented for a user at some
point. It is up to the front-end implementation to decide which log
events to present to the user at all, as well as when and how.

Logging to file may also be done my a separate logging process, or it
can be implemented at each of the various OpenVPN 3 D-Bus services
running. Each D-Bus service will regardless of file logging also send
the Log signal on the D-Bus.

Generic logging design
----------------------

Each log message are put into separate groups, depending on which
OpenVPN 3 D-Bus service is issuing the Log signal. The complete list
of of groups and categories can be found in
`src/log/log-helpers.hpp`.

### Log groups

The log group mapping is defined in the `LogGroup` enum class

| Identifier   | ID | Description                                                                             |
|--------------|:--:|-----------------------------------------------------------------------------------------|
| UNDEFINED    | 0  | Invalid value, should not occur                                                         |
| MASTERPROC   | 1  | Not in use, can be recycled                                                             |
| CONFIGMGR    | 2  | Log messages sent by the configuration manager (`openvpn3-service-configmgr`)           |
| SESSIONMGR   | 3  | Log messages sent by the session manager (`openvpn3-service-sessionmgr`)                |
| BACKENDSTART | 4  | Log messages sent by the backend process starter (`openvpn3-service-backendstart`)      |
| LOGGER       | 5  | Log messages sent by the `openvpn3-service-logger` process or similar logging services. |
| BACKENDPROC  | 6  | Log messages sent by the `openvpn3-service-client` processes.                           |
| CLIENT       | 7  | Log messages coming from the OpenVPN 3 Core library's client implementation. These are sent by the `openvpn3-service-client` process, but tagged separately to differentiate better what is VPN connection related and what is client process related. |


### Log categories

The log category mapping is defined in the `LogCategory` enum class.

| Identifier   | ID  | Description                                                        |
|--------------|:---:|--------------------------------------------------------------------|
| DEBUG        | 0   | Mostly messages useful for debugging and during development, carries quite implementation specific details |
| VERB2        | 1   | More verbose messages                                              |
| VERB1        | 2   | Verbose messages                                                   |
| INFO         | 3   | Nice to know, informational messages                               |
| WARNING      | 4   | Warnings which should normally be fixed but should not impact the general operational state |
| ERROR        | 5   | Issues which may impact the operational state                      |
| CRITICAL     | 6   | Critical issues which will have an impact on the operational state |
| FATAL        | 7   | Fatal errors which makes the process stop                          |


D-Bus Log signals
-----------------

Each log signal carries the information described in the section above, but each signal also contains a reference to the interface and object path of the sender.

The `Log` signals provides these variables:

| Name      | Type   | Description                                    |
|-----------|--------|------------------------------------------------|
| group     | uint   | Which log group this signal belongs to         |
| level     | uint   | Which log verbosity level this message carries |
| message   | string | The log message itself                         |
