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

# Policy files to build
SELINUX_POLICIES = openvpn3.pp openvpn3_service.pp


#
#  Generic SELinux policy build
#

EXTRA_DIST = \
	$(patsubst %.pp,%.te,$(SELINUX_POLICIES)) \
	$(patsubst %.pp,%.if,$(SELINUX_POLICIES)) \
	$(patsubst %.pp,%.fc,$(SELINUX_POLICIES))


if ENABLE_SELINUX_BUILD

BUILD_POLICIES = $(patsubst %.pp,%.pp.bz2,$(SELINUX_POLICIES))

all : $(BUILD_POLICIES)

MAINTAINERCLEANFILES = Makefile Makefile.in $(BUILD_POLICIES) $(SELINUX_POLICIES)

CLEANFILES = $(BUILD_POLICIES) $(SELINUX_POLICIES)

# The three files being copied first happens only when doing out-of-tree builds,
# as the SELinux build method does not account for such scenarios
%.pp :
	test $(top_srcdir) != $(top_builddir) && cp -v $(patsubst %.pp,$(top_srcdir)/src/selinux/%.te,$@) . || true
	test $(top_srcdir) != $(top_builddir) && cp -v $(patsubst %.pp,$(top_srcdir)/src/selinux/%.if,$@) . || true
	test $(top_srcdir) != $(top_builddir) && cp -v $(patsubst %.pp,$(top_srcdir)/src/selinux/%.fc,$@) . || true
	make -f ${selinux_devel_path}/Makefile $@

%.pp.bz2 : %.pp
	$(AM_V_GEN) bzip2 -f9 $<

install-data-local : $(BUILD_POLICIES)
	$(MKDIR_P) "$(DESTDIR)$(prefix)/share/selinux/packages"
	$(INSTALL_DATA) $(BUILD_POLICIES) "$(DESTDIR)$(prefix)/share/selinux/packages"

clean-local :
	-rm -rf tmp/
	test $(top_srcdir) != $(top_builddir) && rm -fv $(EXTRA_DIST) || true

uninstall-local :
	@for f in `echo $(BUILD_POLICIES)`; do rm -vf "$(DESTDIR)$(prefix)/share/selinux/packages/$$f" ; done

endif # ENABLE_SELINUX_BUILD
