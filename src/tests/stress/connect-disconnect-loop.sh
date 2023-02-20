#!/bin/bash
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2019 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2019 - 2023  David Sommerseth <davids@openvpn.net>
#

##
# @file  connect-disconnect-loop.sh
#
# @brief A stress tester of the session manager, client and netcfg
#        components in the OpenVPN 3 Linux stack.
#
#        This script will iterate 10000 times doing a connect,
#        ping the remote server and disconnect again.  The remote
#        server VPN IP address is expected to be 10.8.0.1.  See
#        the server and client example configuration files for
#        more information.
#
#        If any of the openvpn3 commands or ping fails, the script
#        is expected to exit instantly.  stderr is also redirected
#        to stdout to simplify catching all the logs by a callers
#        of this script.
#

exit_msg()
{
    echo ".............................."
    echo "### ERROR ### Failed in $1 phase"
    echo ".............................."
    exit 2
}

if [ $# -lt 1 ]; then
    echo "Usage: $0 <.ovpn config profile>"
    exit 3
fi

{
    # Generate a unique configuration name to use for this test run
    cfgname="$(basename $0)_${BASHPID}_testcfg"

    # Import the configuration file to the config manager; this
    # test does not try to stress test the configuration manager
    openvpn3 config-import --name "$cfgname" --config "$1" || exit_msg "config-import"

    for i in $(seq 1 10000);
    do
        echo "=========== $i ==============";
        echo -n "Started: "; date;
        echo "";

        # Main loop - start the VPN session, ping and disconnect
        openvpn3 session-start --config "$cfgname" || exit_msg "session-start"
        ping -c 3 -i 0.5 10.8.0.1 || exit_msg "ping"
        openvpn3 session-manage --disconnect --config "$cfgname" || exit_msg "session-disconnect"

        # FIXME: The TUN_PACKETS_OUT and TUN_PACKETS_IN variables printed after the disconnect
        #        should be checked and compared against the number of pings sent.  It should be
        #        pretty much close to the same number, due to the short life time of the session.

        echo -n "Completed: "; date; echo "";
    done

    # Clean up the configuration file imported
    openvpn3 config-remove --force --config "$cfgname" || exit_msg "config-remove"
} 2>&1
