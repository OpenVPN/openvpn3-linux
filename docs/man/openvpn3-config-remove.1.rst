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
| ``openvpn3 config-remove`` ``-o DBUS-PATH`` | ``--path DBUS-PATH`` | ``--config CONFIG-NAME`` ``[--force]``
| ``openvpn3 config-remove`` ``-h`` | ``--help``


DESCRIPTION
===========
This will delete an imported configuration profile from the configuration
manager.

OPTIONS
=======

-h, --help              Print  usage and help details to the terminal

-o DBUS-PATH, --path DBUS-PATH
                        D-Bus configuration path to the configuration to
                        delete.  This can be found in ``openvpn3 configs-list``.

--config-path DBUS-PATH
                        Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                        Can be used instead of ``--path`` where the
                        configuration profile name is given instead.  Available
                        configuration names can be found via
                        ``openvpn3 configs-list``.

--force                 By default, the user must type in a confirmation before
                        the configuration is deleted.  By adding this option,
                        the configuration will be deleted instantly without
                        any user interaction.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-import``\(1)
``openvpn3-configs-list``\(1)
