======================
openvpn3-config-import
======================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-import`` ``-c FILE`` | ``--config FILE`` ``[-n NAME | --name NAME]`` ``[-p | --persistent]``
| ``openvpn3 config-import`` ``-h`` | ``--help``


DESCRIPTION
===========
Any VPN connection is based on a VPN configuration profile, which are managed
by the ``OpenVPN 3 Configuration Manager``.  The ``openvpn3 config-import``
command enables pre-loading a configuration file into the configuration manager
where additional host specific adjustments can be added on top of the imported
configuration, in addition to grant access to the profile to other users.

OPTIONS
=======

-h, --help               Print  usage and help details to the terminal

-c FILE, --config FILE   File name of the configuration profile to import

-n NAME, --name NAME     Each configuration profile carries a human readable
                         name.  The default value is the configuration profile
                         file name, but can be amended during import with this
                         option.

-p, --persistent         By default, the configuration profile is kept in
                         memory only, and it is wiped when the configuration
                         manager service restarts.  When adding this option the
                         profile becomes persistent and will be automatically
                         loaded each time the configuration manager starts.

SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-manage``\(1)
``openvpn3-config-acl``\(1)
``openvpn3-config-remove``\(1)
``openvpn3-config-show``\(1)
``openvpn3-configs-list``\(1)
``openvpn3-session-start``\(1)

