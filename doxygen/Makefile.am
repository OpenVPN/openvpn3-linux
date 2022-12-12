#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  SPDX-License-Identifier: AGPL-3.0-only
#
#  Copyright (C) 2017 - 2022  OpenVPN Inc <sales@openvpn.net>
#  Copyright (C) 2017 - 2022  David Sommerseth <davids@openvpn.net>
#

MAINTAINERCLEANFILES = \
	$(srcdir)/Makefile.in

DISTCLEANFILES = openvpn3-linux.doxy

.PHONY: doxygen
doxygen: openvpn3-linux.doxy
	doxygen openvpn3-linux.doxy

clean-local:
	-rm -rf html latex
