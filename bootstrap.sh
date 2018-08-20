#!/bin/sh
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 3 of the License
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

set -eu

# if pkg-config is not installed you get the extremely obscure error from autoconf
#   error: possibly undefined macro: AC_MSG_ERROR
# error out here to make this a bit easier

if ! pkg-config --version > /dev/null; then
 echo "pkg-config is required";
 exit 1
fi


echo "** Initializing git submodules ..."
git submodule init
git submodule update
echo

echo "** Updating version.m4 ..."
./update-version-m4.sh
echo

echo "** Running autoreconf ..."
autoreconf -vi
echo
