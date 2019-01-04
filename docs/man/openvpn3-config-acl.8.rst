===================
openvpn3-config-acl
===================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 8
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-acl`` ``-o D-BUS_PATH`` | ``--path D-BUS_PATH`` ``[OPTIONS]``
| ``openvpn3 config-acl`` ``-h`` | ``--help``


DESCRIPTION
===========
Each configuration profile has its own Access Control List associated with it.
This enables a configuration profile to be shared by more users in a more
controlled way, where specific or all users can be granted access to start
new VPN sessions of pre-loaded configurations.  It can also be used to restrict
users from viewing the contents of the configuration profile while still being
able to start VPN sessions.

All options below can be used together.  If the ``--show`` option is used, it
will list the current Access Control List after any changes has been performed.

OPTIONS
=======

-h, --help               Print  usage and help details to the terminal

-o D-BUS_PATH, --path D-BUS_PATH
                        D-Bus configuration path to the configuration to manage
                        the ACL.  This can be found in
                        ``openvpn3 configs-list``.

-s, --show              Show the currently active ACL.

-G UID|USERNAME, --grant UID|USERNAME
                        Grant the given user ID (UID) or username read-only
                        access to this configuration profile.

-R UID|USERNAME, --revoke UID|USERNAME
                        Revoke access to this configuration profile from the
                        given user ID (UID) or username.

--public-access true|false
                        Grant all users on the system read-only access to
                        this configuration profile.  This effectively disables
                        the more fine-grained access control provided via
                        ``--grant``.

--lock-down true|false  Locks down the configuration profile so it can only
                        be used to start new VPN sessions by users granted
                        access.  Only the configuration profile owner can now
                        show the profile contents via ``openvpn3 config-show``.

-S, --seal              This seals the configuration profile for everyone,
                        making the configuration profile effectively read-only,
                        even for the owner.  A configuration profile *cannot* be
                        unsealed.


SEE ALSO
========

``openvpn3``
``openvpn3-config-import``
``openvpn3-config-manage``
``openvpn3-config-show``
``openvpn3-configs-list``
