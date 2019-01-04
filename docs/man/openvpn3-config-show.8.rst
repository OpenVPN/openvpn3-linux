====================
openvpn3-config-show
====================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-show`` ``-o D-BUS_PATH`` | ``--path D-BUS_PATH`` ``[--json]``
| ``openvpn3 config-show`` ``-h`` | ``--help``


DESCRIPTION
===========
This will dump the contents of the OpenVPN configuration profile which was
imported to the terminal.  It will not carry any information added via
``openvpn3 config-manage``.

If the configuration profile has been locked down
via ``openvpn3 config-acl`` users granted access to this configuration profile
will not be able to see the contents of the configuration profile.


OPTIONS
=======

-h, --help               Print  usage and help details to the terminal
-o D-BUS_PATH, --path D-BUS_PATH    D-Bus configuration path to the
                         configuration to show.  This can be found in
                         ``openvpn3 configs-list``.
--json                   Use JSON formatting of the output instead of
                         text/plain.


SEE ALSO
========

``openvpn3``
``openvpn3-config-acl``
``openvpn3-config-import``
``openvpn3-configs-list``
