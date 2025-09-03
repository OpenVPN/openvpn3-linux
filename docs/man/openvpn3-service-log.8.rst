====================
openvpn3-service-log
====================

--------------------------------------
OpenVPN 3 Linux - Internal Log Service
--------------------------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-service-log`` ``[OPTIONS]``
| ``openvpn3-service-log`` ``-h`` | ``--help``


DESCRIPTION
===========
This program can run in two modes; as a stand-alone program without any
services enabled or as a D-Bus service.  By default the
``openvpn3-service-log`` process will be started automatically as a D-Bus
service when any of the OpenVPN 3 Linux service backends wants to send log
events.  The auto-start is handled by the ``dbus-daemon`` via the
*net.openvpn.v3.log.service* configuration file.

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

--log-level LEVEL
                Sets the system wide log verbosity for the log events being
                logged to file or any other log destination
                ``openvpn3-service-log`` is configured to use.  Valid values
                are *0* to *6*.  The higher value, the more verbose the log
                events will be.  Log level *6* will retrieve all debug events.

                Sets the system wide log verbosity for log events being logged
                to file or any other log destination
                ``openvpn3-service-log`` is configured to use.
                The default is :code:`3`.  Valid values are :code:`0` to
                :code:`6`.  Higher log levels results in more verbose logs and
                log level :code:`6` will contain all debug log events.
                This log-level can setting can be modified at runtime via
                ``openvpn3-admin log-service`` if running with ``--service``.

--log-file FILE
                This will write all log events to *FILE* instead of the
                terminal.

--journald
                This will make all log events be sent to the systemd-journald\(8)
                log service.  This approach will add additional meta data to the
                journal log to each log event.  Additional journal fields added
                are prefixed with ``O3_``.  Examples of such fields are:
                ``O3_LOGTAG``, ``O3_SENDER``, ``O3_INTERFACE``, ``O3_OBJECT_PATH``,
                ``O3_LOG_GROUP`` and ``O3_LOG_CATEGORY``.

                To view this information, use the ``--output-fields=`` option to
                ``journalctl``\(1).

                Example:
                ::

                    # journalctl --output-fields=O3_LOGTAG,O3_OBJECT_PATH,O3_INTERFACE \
                          O3_LOG_GROUP,MESSAGE -o verbose

--no-logtag-prefix
                This option is only effective together with ``--journald``.  The
                default is to prefix each log line (``MESSAGE=``) with a tag value
                unique to the sender (see ``openvpn3-admin log-service --list-subscriptions``).
                With journald logging, this log tag is already preserved as a journal
                meta filed (``O3_LOGTAG``).  By enabling this option, this log tag
                prefixing to the log message line will not happen.

--syslog
                This will make all log events be sent to the generic system
                logger via the ``syslog``\(3) function.

--syslog-facility LOG-FACILITY
                To be used together with --syslog.  The default *LOG-FACILITY*
                is *LOG_DAEMON*.  For other valid facilities, see the
                *facility* section in ``syslog``\(3).

--service-log-dbus-details
                Each log event contains some more detailed meta-data of the
                sender of the log event.  This is disabled by default, but when
                enabled it will add a line on the log destination
                before the log event itself with this meta-data.  This is mostly
                only useful when debugging and not recommended for normal
                production.

--idle-exit MINUTES
                The ``openvpn3-service-log`` service will exit automatically
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
