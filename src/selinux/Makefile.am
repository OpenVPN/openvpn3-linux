#  OpenVPN 3 Linux client -- Next generation OpenVPN client
#
#  Copyright (C) 2018 - 2020  OpenVPN Inc. <sales@openvpn.net>
#  Copyright (C) 2018 - 2020  David Sommerseth <davids@openvpn.net>
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

# The SELinux build method doesn't like parallel builds, so disable it here
.NOTPARALLEL:

if ENABLE_SELINUX_BUILD
HEADERDIR = ${selinux_devel_path}/include
include $(HEADERDIR)/Makefile
endif

SELINUX_POLICIES = openvpn3.pp.bz2

MAINTAINERCLEANFILES = Makefile Makefile.in $(SELINUX_POLICIES)

CLEANFILES = $(SELINUX_POLICIES)

EXTRA_DIST = \
	openvpn3.te \
	openvpn3.if \
	openvpn3.fc

%.pp.bz2 : %.pp
	$(AM_V_GEN) bzip2 -f9 $<

install-data-local : $(SELINUX_POLICIES)
	$(MKDIR_P) "$(DESTDIR)$(prefix)/share/selinux/packages"
	$(INSTALL_DATA) $(SELINUX_POLICIES) "$(DESTDIR)$(prefix)/share/selinux/packages"

clean-local :
	-rm -rf tmp/

uninstall-local :
	@for f in `echo $(SELINUX_POLICIES)`; do rm -vf "$(DESTDIR)$(prefix)/share/selinux/packages/$$f" ; done
