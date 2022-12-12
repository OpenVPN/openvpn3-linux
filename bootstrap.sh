#!/bin/sh
#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
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
