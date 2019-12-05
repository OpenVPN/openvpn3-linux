#!/bin/bash
#
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2019         OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2019         David Sommerseth <davids@openvpn.net>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as
#  published by the Free Software Foundation, version 3 of the
#  License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
