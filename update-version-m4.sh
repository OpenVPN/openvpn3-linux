#!/bin/sh
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2017 - 2019  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2017 - 2019  David Sommerseth <davids@openvpn.net>
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

set -u

#
#  Retrieve the version based on the git tree
#
#  If a version tag is found (always prefixed with 'v'), this value
#  is used.  Otherwise compose a version string based on branch name
#  and commit reference
VERSION="$(git describe --always --tags)"

#  Only accept explicit tag references, not a reference
#  derived from a tag reference, such as "v3_beta-36-gcd68aee"
echo ${VERSION} | grep -qE "^v[[:digit:]]+(|_[[:alnum:]]+)$"
ec=$?
set -eu
if [ $ec -ne 0  ]; then
	# Presume not a version tag, so use commit reference
	VERSION="$(git rev-parse --symbolic-full-name HEAD | cut -d/ -f3-)_$(git rev-parse --short=16 HEAD)"
	GUIVERSION="$(echo $VERSION | sed 's/_/:/')"
	PRODVERSION="$(echo $VERSION | sed 's#/#_#g')"
else
	# bash could use ${VERSION:1}, but we try to make it
	# work with more basic sh - so we use 'cut' to get the
	# same result
	PRODVERSION="$(echo ${VERSION} | cut -b2-)"
	GUIVERSION="${VERSION}"
fi
echo "Version: $VERSION"

# Generate version.m4
{
    cat <<EOF
define([PRODUCT_NAME], [OpenVPN 3/Linux])
define([PRODUCT_VERSION], [${PRODVERSION}])
define([PRODUCT_GUIVERSION], [${GUIVERSION}])
define([PRODUCT_TARNAME], [openvpn3-linux])
define([PRODUCT_BUGREPORT], [openvpn-devel@lists.sourceforge.net])
EOF
} > version.m4

# Ensure the config-version.h file gets updated
rm -f config-version.h
