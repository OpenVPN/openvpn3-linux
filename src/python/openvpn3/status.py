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
# @file  status.py
#
# @brief  Parses OpenVPN 3 Session object status property results
#

from openvpn3.constants import StatusMajor, StatusMinor

##
#  Translates a D-Bus object's status property into
#  Python constants.
#
#  @param status  The full contents of the session objects status property
#
#  @returns A tuple of (StatusMajor, StatusMinor, status_message)
#
def ParseStatus(status):
    return (StatusMajor(status['major']), StatusMinor(status['minor']),
            status['status_message'])

