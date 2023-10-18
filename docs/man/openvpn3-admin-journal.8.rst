==========================
openvpn3-admin-journal
==========================

---------------------------------------------
OpenVPN 3 Linux - systemd-journald log viewer
---------------------------------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3-admin journal`` ``[OPTIONS]``
| ``openvpn3-admin journal`` ``-h`` | ``--help``


DESCRIPTION
===========
This is a convenience helper command to easily retrieve log entries for
the OpenVPN 3 Linux stack.  This command depends on the ``net.openvpn.v3.log``
service being configured to use the ``systemd-journald``\(8) service for
logging.

The log entries presented using this command will only extract log entries
for the OpenVPN 3 Linux backend D-Bus services.


OPTIONS
=======

-h, --help      Print  usage and help details to the terminal

--json
                By default the extracted log will be in traditional plain
                text format.  This option will change the output to be a
                more verbose JSON format, which will include far more
                details for each log entry.

--since TIMESTAMP
                Without this being provided, it will retrieve all log
                entries available in the systemd-journal.  The ``TIMESTAMP``
                format is not strict, but the order of the values
                are important.  The keywords ``today`` and ``yesterday``
                are also valid.

                These are all valid time-stamp values:

                ::

                        --since 2022
                        --since 2022-11
                        --since 2022-12-05
                        --since "2022-12-05 15:00"
                        --since "2022-12-05 15:00:40"
                        --since today
                        --since yesterday

                The first line will extract all log entries available
                starting with January 1, 2022.  The second example
                will take all entries starting from November 1.  The
                third one takes all log lines after December 5, 2022.

                Using ``today`` will be the same as using today's date
                and ``yesterday`` will use date before today.  Both retrieves
                all entries from midnight that day.

--path DBUS_PATH
                All OpenVPN 3 services, configuration profiles and VPN
                sessions uses unique D-Bus object paths.  This information
                is stored in the systemd-journald, and you can retrieve
                log entries for only a specific service, configuration or
                VPN session.

                ::

                        --path /net/openvpn/v3/configuration/....
                        --path /net/openvpn/v3/sessions/....

                The currently available paths can be found using the
                ``openvpn3 configs-list``\(1) and
                ``openvpn3 sessions-list``\(1) commands.  Older paths
                can also be used, as long as they are still available in
                the ``systemd-journald``.  These paths are also
                available when retrieving them with JSON formatting.

--sender DBUS_NAME
                This is similar to the D-Bus path, each D-Bus service
                are given a unique bus name.  This bus name counter is
                reset when the host is rebooted.  Currently available
                bus names related to OpenVPN 3 Linux can be found by
                running ``openvpn3-admin log-service --list-subscriptions``.

--interface DBUS_INTERFACE
                This is yet another approach to filter out specific
                D-Bus services, but this filter is less specific.  This
                will retrieve all records for a service, regardless of
                time, D-Bus path or bus name.

                Some commonly used D-Bus interfaces:

                ::

                        net.openvpn.v3.backends
                        net.openvpn.v3.configuration
                        net.openvpn.v3.log
                        net.openvpn.v3.netcfg
                        net.openvpn.v3.netcfg.core
                        net.openvpn.v3.sessions

--logtag LOGTAG
                Each OpenVPN 3 Linux backend service which wants to
                send log events will be assigned a LogTag value.  This
                is a fairly unique value.  The currently used LogTag
                values can be extracted by running
                ``openvpn3-admin log-service --list-subscriptions``.  Older
                values can also be extracted as long as they can be found in
                the ``systemd-journald``.

--session-token TOKEN
                This is specific to retrieve log events for a specific VPN
                session.  The TOKEN value is the value given to the
                ``openvpn3-service-client`` process.  This will only extract
                log entries for the VPN client process itself and no other
                support services (such as ``openvpn3-service-netcfg``).

All of these filters can be combined to narrow down the amount of log data.


SEE ALSO
========

``openvpn3-service-logger``\(8)
