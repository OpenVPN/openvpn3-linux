======================
openvpn3-config-remove
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-remove`` ``-o D-BUS_PATH`` | ``--path D-BUS_PATH`` ``[--force]``
| ``openvpn3 config-remove`` ``-h`` | ``--help``


DESCRIPTION
===========
This will delete an imported configuration profile from the configuration
manager.

OPTIONS
=======

-h, --help               Print  usage and help details to the terminal
-o D-BUS_PATH, --path D-BUS_PATH    D-Bus configuration path to the
                         configuration to delete.  This can be found in
                         ``openvpn3 configs-list``.
--force                  By default, the user must type in a confirmation before
                         the configuration is deleted.  By adding this option,
                         the configuration will be deleted instantly without
                         any user interaction.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-import``\(1)
``openvpn3-configs-list``\(1)
