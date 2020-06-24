=======================
openvpn3-service-logger
=======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-service-logger`` ``[OPTIONS]``
| ``openvpn3-service-logger`` ``-h`` | ``--help``


DESCRIPTION
===========
This program can run in two modes; as a stand-alone program without any
services enabled or as a D-Bus service.  By default the
``openvpn3-service-logger`` process will be started automatically as a D-Bus
service when any of the OpenVPN 3 Linux service backends wants to send log
events.  The auto-start is handled by the ``dbus-daemon`` via the
*net.openvpn.v3.logger.service* configuration file.

When run as a stand-alone program, it is limited in what kind of log events it
will receive.  The other backend services need to use the ``--signal-broadcast``
option for this program to pick up log events as they occur.  The drawback of
this approach is that everyone on the system, may listen to the various log
signals happening.  This operational mode is therefore more useful when
debugging the various OpenVPN 3 Linux services or it is believed to be an issue
with the related D-Bus policies.

If run as a D-Bus service, each of the various backend services will attach
themselves to this log service before starting to send log events.  This has
a higher security profile and restricts log event access to only this program.

The default behaviour is to print log events to the terminal, unless otherwise
configured via options provided during the start-up.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--version       Prints the version of the program and exists

--timestamp
                This enables printing timestamps to the log output.  This is
                mostly useful when sending log data to file or terminal.  When
                using other system log services (such as syslog), timestamps
                are often added by those log services instead.

--colour
                When logging to terminal or file, this adds ANSI colour escape
                codes to the log data, to more easily separate various log
                events from each other.  Log event colours are grouped by the
                log level of the log event.

--config-manager
                This is used when running as a stand-alone program.  This will
                subscribe to log events sent by the
                ``openvpn3-service-configmgr`` process.  Remember that the
                configuration manager must be started with
                ``--signal-broadcast`` for log events to be received this way.

--session-manager
                This is used when running as a stand-alone program.  This will
                subscribe to log events sent by the
                ``openvpn3-service-sessionmgr`` process.  Remember that the
                session manager must be started with
                ``--signal-broadcast`` for log events to be received this way.

--session-manager-client-proxy
                The session manager can proxy log events from the backend
                VPN client process to users who is granted access to log events
                from specific VPN sessions.  This extends the
                ``--session-manager`` subscription to also include log events
                it would proxy as well.  This will only happen for VPN sessions
                which has the boolean :code:`receive_log_events` flag set in
                the D-Bus session object.

--vpn-backend
                This is used when running as a stand-alone program.  This will
                subscribe to log events sent by ``openvpn3-service-client``
                processes, also known as the backend VPN client processes.
                This requires that the ``openvpn3-service-backendstart`` process
                is started with ``--client-signal-broadcast``, which ensures
                ``openvpn3-service-client`` is started correctly and adds the
                ``signal-broadcast`` argument as well.

--log-level LEVEL
                Sets the system wide log verbosity for the log events being
                logged to file or any other log destination
                ``openvpn3-service-logger`` is configured to use.  Valid values
                are *0* to *6*.  The higher value, the more verbose the log
                events will be.  Log level *6* will retrieve all debug events.

                Sets the system wide log verbosity for log events being logged
                to file or any other log destination
                ``openvpn3-service-logger`` is configured to use.
                The default is :code:`3`.  Valid values are :code:`0` to
                :code:`6`.  Higher log levels results in more verbose logs and
                log level :code:`6` will contain all debug log events.
                This log-level can setting can be modified at runtime via
                ``openvpn3-admin log-service`` if running with ``--service``.

--log-file FILE
                This will write all log events to *FILE* instead of the
                terminal.

--syslog
                This will make all log events be sent to the generic system
                logger via the ``syslog``\(3) function.

--syslog-facility LOG-FACILITY
                To be used together with --syslog.  The default *LOG-FACILITY*
                is *LOG_DAEMON*.  For other valid facilities, see the
                *facility* section in ``syslog``\(3).

--service
                This will start ``openvpn3-service-logger`` as a D-Bus service,
                which log senders can attach their log streams to.  In this
                mode, further adjustments to the running behaviour can be
                managed via ``openvpn3-admin log-service``.

--service-log-dbus-details
                Each log event contains some more detailed meta-data of the
                sender of the log event.  This is disabled by default, but when
                enabled it will add a line on the log destination
                before the log event itself with this meta-data.  This is mostly
                only useful when debugging and not recommended for normal
                production.

--idle-exit MINUTES
                The ``openvpn3-service-logger`` service will exit automatically
                if it is being idle for *MINUTES* minutes.  By being idle, it
                means no other services have attached their log streams to this
                service.  To see how many log subscriptions are attached, see
                the output of ``openvpn3 log-service``.

--state-dir DIRECTORY
                When this option is given, it will save the current runtime
                settings in a file inside this directory.  This is used to
                preserve the log settings across process restarts, for example
                if the ``--idle-exit`` kicks in or the host is rebooted.  The
                contents of this file is not expected to be modified directly
                but rather use the ``openvpn3-admin log-service`` as the
                configuration tool.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-admin-log-service``\(1)
``syslog``\(3)
