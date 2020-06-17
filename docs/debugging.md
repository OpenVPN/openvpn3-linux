Debugging OpenVPN 3 Linux
=========================

Since all of the backend processes of the OpenVPN 3 Linux client is started
automatically by D-Bus (through the auto-start service), it gets harder to debug
what is happening straight out-of-the-box.  But several tweaks have been added
to ease this.

First, the OpenVPN 3 Linux client must be compiled with debug options.  This is
done by running `./configure` with the `--enable-debug-options` argument.
It is also advisable to ensure the various tools under `src/tests/dbus` have
been built as well.

There are six backend services to beware of.

  1. `openvpn3-service-configmgr`
  2. `openvpn3-service-sessionmgr`
  3. `openvpn3-service-backendstart`
  4. `openvpn3-service-client`
  5. `openvpn3-service-netcfg`
  6. `openvpn3-service-logger`

## Running backend services in the terminal

All of these, with the exception of 4. `openvpn3-service-client` can be
started from the command line.  They will all have a idle time-out mechanism,
which means it will exit after some idle time if the service is not in use.
This can be disabled by adding `--idle-exit 0` to the command line.

All of these services can be started as `openvpn` with the default D-Bus
policy, with the exception of `openvpn3-service-netcfg` which must be
started as root - but it will fairly quickly drop all but the needed
capabilities and switch to the `openvpn` user as well.  If the other
services are being started as `root`, they will also switch to `openvpn`
automatically.

This also means five of backend services can be run via external debug tools
directly on the command line.  The `openvpn3-service-client` is different,
as that needs to be started via `openvpn3-service-backendstart`.

## Debugging openvpn3-service-client

It is possible to attach a debugger to `openvpn3-service-client` as well, by
running `openvpn3-service-backendstart` on the command line together with a
few extra arguments.  Just remember, this service **must** be started and run as
`openvpn` for everything to function correctly.

     # openvpn3-service-backendstart --idle-exit 0 \
                --run-via $DEBUGGER               \
                --debugger-arg $DBG_ARG1          \
                --debugger-arg $DBG_ARG2          \
                --debugger-arg $DBG_ARG3

In some situations, it might not be wanted to have the
`openvpn3-service-client` to daemonize and start a new process session id
(`setsid(3P)`).  This can be avoided by also adding `--client-no-fork` and
`--client-no-setsid` to the command line above.

To run `openvpn3-service-client` via `valgrind`, you could do like this:

    # openvpn3-service-backendstart --idle-exit 0 \
                --run-via /usr/bin/valgrind       \
                --debugger-arg "--leak-check=full"

To run `openvpn3-service-client` via GDB, a different approach needs to be
taken - by using the remote debugging feature of GDB.

    # openvpn3-service-backendstart --idle-exit 0 \
                --client-no-fork                   \
                --client-no-setsid                 \
                --run-via /usr/bin/gdbserver       \
                --debugger-arg localhost:9944

In a different terminal, start `gdb` like this:

    # gdb openvpn3-service-client
    [...snip...]
    (gdb) target remote localhost:9944

When the session manager (`openvpn3-service-sessionmgr`) starts a new
tunnel, the terminal with `gdb` running will come back with a prompt where you
can continue the execution.

### Caveats with GDB
D-Bus is fairly sensitive to time-outs.  These time-outs are normally reasonable
but you might hit several time-outs when using this way of debugging.  Further,
by using `--client-no-fork` it may also happen that various openvpn3
front-ends will not respond as expected.  In these cases, using the `openvpn3`
Python module might be of help, where it is possible to step through each of
the various steps in a more controlled manner; see below for details.


## D-Bus debugging via glib2

By defining the `G_DBUS_DEBUG` environment variable, it is possible to
inspect the various D-Bus messages being passed to/from a glib2 based D-Bus
service or client.  The most verbose debug logging is gained with using
`G_DBUS_DEBUG=all`.  For more details see the upstream Glib2
[Running GIO applications: GIO Reference Manual](https://developer.gnome.org/gio/stable/running-gio-apps.html) documentation.

To pass the `G_DBUS_DEBUG` variable to the `openvpn3-service-client` process,
the `openvpn3-service-backendstart` need to be started with
`--client-setenv G_DBUS_DEBUG=all`, which will dump all D-Bus operations the
openvpn3-service-client process handles to the console.


## More fine grained session management control

It is fully possible to get a more fine grained control of starting tunnels.
The easiest way is by using Python and the openvpn3 module.  It is advisable to
first import the OpenVPN configuration profile via `openvpn3 config-import`.
This will give you a configuration D-Bus path which can be easily used further.

    #!/usr/bin/python3
    
    import dbus
    import openvpn3
    
    # Shared D-Bus System Bus connection
    sysbus = dbus.SystemBus()
    
    # Get access to the configuration manager
    cfgmgr = openvpn3.ConfigurationManager(sysbus)
    
    # Retrieve access to the configuration profile
    cfg = cfgmgr.Retrieve('/net/openvpn/v3/configuration/some-path')
    
    
    # Get access to the session manager
    sessmgr = openvpn3.SessionManager(sysbus)
    
    # Create a new VPN session, based on the retrieved configuration
    sess = sessmgr.NewTunnel(cfg)
    print("Session path: %s" % sess.GetPath())
    
    # Various actions to do on the session object
    sess.Ready()      # Ready for connection?
    sess.Connect()    # Start a connection
    sess.Pause()      # Pause the connection
    sess.Resume()     # Resume the connection
    sess.Disconnect() # Disconnect and close the session.  The session
                      # object is invalid after this call.

The methods available in the configuration manager object (`cfgmgr`),
configuration object (`cfg`), session manager object (`sessmgr`) and the
session object (`sess`) mostly works in the same way and with the same names
as the D-Bus raw API for these objects.  The most noticeable difference is the
`Retrieve(path)` and `GetPath()`methods as well as the interface for
providing username/password credentials to a session object.  See
[`src/tests/python`](../src/tests/python) for more examples.

## Other challenges
D-Bus by design is quite strict when it comes to services using the system
bus.  This means it can quite often happen that D-Bus calls or signals are
being rejected by the D-Bus daemon.  The best way to detect these issues, is
to look into the D-Bus logs.  On systems with systemd, this is easily done
via `journalctl --since today -u dbus`.  All log events processed by
`openvpn3-service-logger` will typically also be present here.


## Logging

Almost all log events happens exclusively over D-Bus.  Some of these log events
are targeted to either the session manager or the `openvpn3-service-logger`
service.  To retrieve logs it is therefore needed to run
`openvpn3-service-logger` with the `--service` argument as the `openvpn`
user.  This ensures that the services will fetch log entries directly.

If no log events happens with `openvpn3-service-logger`, the
`openvpn3-service-backendstart` can be run with
`--client-signal-broadcast`.  Enabling this will send all backend client
signals as system wide D-Bus broadcast signals.

In addition there are more log tools under `./src/tests/dbus`.

  * `signal-listener`:  Dumps almost all D-Bus signals broadcasts on the
     system.  This is quite low-level and will not show any "targeted"
     signals towards a specific recipient.  It will also decode some
     of the OpenVPN 3 specific D-Bus signals.

  * `openvpn3 log --log-level 6 --session-path ${SESSION_DBUS_PATH}`: This
     will enable the debug logging for a specific running VPN session.

  * `log-listener`, `log-listener2`:  Variants of `signal-listener` which
     only listens for `Log` signals.

  * `logservice1` and `openvpn3 log-service`:  Can be used to query and
     modify properties in the `openvpn3-service-logger` service.  The
     `logservice1` can also be used to generate some log events.

  * `enable-logging`: Used to enable Log forwarding from the
    `openvpn3-service-client` backend to the session manager.  The session
     manager will then proxy these log events to a front-end user
     (as `Log` signals).
