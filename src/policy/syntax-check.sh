#!/bin/bash
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
#

set -eu

for f in "$(dirname $0)"/*.conf;
do
        echo -n "-- Checking $f ... "
        xmllint --format $f > /dev/null || exit $? && echo "PASS"
done
