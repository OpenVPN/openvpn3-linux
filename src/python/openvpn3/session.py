#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017 - 2018  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017 - 2018  David Sommerseth <davids@openvpn.net>
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
# @file  session.py
#
# @brief  Classes and functions related to OpenVPN 3 sessions
#


def PrintSessionStatistics(session_properties):
    statistics = session_properties.Get('net.openvpn.v3.sessions',
                                        'statistics')
    if len(statistics) > 0:
        print('Connection statistics:')
        for (key, val) in statistics.items():
            print('    %25s: %i' % (key, val))
