====================
openvpn3-config-dump
====================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-dump`` ``-o DBUS-PATH`` | ``--path DBUS-PATH`` | ``--config CONFIG-NAME`` ``[--json]``
| ``openvpn3 config-dump`` ``-h`` | ``--help``
| ``openvpn3 config-show`` ``-o DBUS-PATH`` | ``--path DBUS-PATH`` | ``--config CONFIG-NAME`` ``[--json]``
| ``openvpn3 config-show`` ``-h`` | ``--help``


DESCRIPTION
===========
This will dump the contents of the OpenVPN configuration profile which was
imported to the terminal.  It will not carry any information added via
``openvpn3 config-manage``.

If the configuration profile has been locked down
via ``openvpn3 config-acl`` users granted access to this configuration profile
will not be able to see the contents of the configuration profile.

This command used to be ``openvpn3 config-show`` but that has been deprecated
in favour of ``openvpn config-dump``.


OPTIONS
=======

-h, --help              Print  usage and help details to the terminal

-o DBUS-PATH, --path DBUS-PATH
                        D-Bus configuration path to the
                        configuration to show.  This can be found in
                        ``openvpn3 configs-list``.

--config-path DBUS-PATH
                        Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                        Can be used instead of ``--path`` where the
                        configuration profile name is given instead.  Available
                        configuration names can be found via
                        ``openvpn3 configs-list``.

--json                  Use JSON formatting of the output instead of
                        text/plain.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-acl``\(1)
``openvpn3-config-import``\(1)
``openvpn3-configs-list``\(1)
