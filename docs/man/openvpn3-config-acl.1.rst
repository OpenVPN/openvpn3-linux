===================
openvpn3-config-acl
===================

----------------------
OpenVPN 3 Linux client
----------------------

:Manual section: 1
:Manual group: OpenVPN 3 Linux

SYNOPSIS
========
| ``openvpn3 config-acl`` ``-o DBUS-PATH`` | ``--path DBUS-PATH`` | ``--config CONFIG-NAME`` ``[OPTIONS]``
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

-o DBUS-PATH, --path DBUS-PATH
                        D-Bus configuration path to the configuration to manage
                        the ACL.  This can be found in
                        ``openvpn3 configs-list``.

--config-path DBUS-PATH
                        Alias for ``--path``.

-c CONFIG-NAME, --config CONFIG-NAME
                        Can be used instead of ``--path`` where the
                        configuration profile name is given instead.  Available
                        configuration names can be found via
                        `openvpn3 configs-list``.

-s, --show              Show the currently active ACL.

-G USER, --grant USER
                        Grant the given user read-only access to this
                        configuration profile.  The USER argument can be either
                        UID or username belonging to the system.

-R USER, --revoke USER
                        Revoke access on this configuration profile for the
                        given user.  The USER argument can be either UID or
                        username belonging to the system.

--public-access BOOL
                        Grant all users on the system read-only access to
                        this configuration profile.  This effectively disables
                        the more fine-grained access control provided via
                        ``--grant``.  Valid argument values: :code:`true`,
                        :code:`false`

-T BOOL, --transfer-owner-session BOOL
                        If another user is granted access to the configuration
                        profile, that user will be the owner of the VPN session
                        when started.  Setting this flag to true will transfer
                        the ownership back to the profile owner while granting
                        the user starting the session rights to also manage the
                        session.  See ``openvpn3-session-acl``\(1) for details
                        on the session ACLs.

--lock-down BOOL
                        Locks down the configuration profile so it can only
                        be used to start new VPN sessions by users granted
                        access.  Only the configuration profile owner can now
                        show the profile contents via ``openvpn3 config-dump``.
                        Valid argument values: :code:`true`, :code:`false`

-S, --seal              This seals the configuration profile for everyone,
                        making the configuration profile effectively read-only,
                        even for the owner.  A configuration profile *cannot* be
                        unsealed.


SEE ALSO
========

``openvpn3``\(1)
``openvpn3-config-import``\(1)
``openvpn3-config-manage``\(1)
``openvpn3-config-dump``\(1)
``openvpn3-configs-list``\(1)
``openvpn3-session-acl``\(1)